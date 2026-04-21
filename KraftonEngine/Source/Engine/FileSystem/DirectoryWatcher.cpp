#include "DirectoryWatcher.h"

FDirectoryWatcher::~FDirectoryWatcher()
{
	Stop();
}

bool FDirectoryWatcher::Start(const std::wstring& DirectoryPath)
{
	if (bIsWatching) return false;

	WatchDirectory = DirectoryPath;

	// 디렉토리 핸들 열기 (백업 시맨틱스 및 오버랩 모드 필수)
	hDirectory = CreateFileW(
		WatchDirectory.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL
	);

	if (hDirectory == INVALID_HANDLE_VALUE)
	{
		std::string ErrorMsg = "DirectoryWatcher: Failed to open directory. Code: " + std::to_string(GetLastError()) + "\n";
		OutputDebugStringA(ErrorMsg.c_str());
		return false;
	}

	// 스레드 종료를 알리기 위한 이벤트 생성
	hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	bIsWatching = true;

	// 백그라운드 감시 스레드 시작
	WatchThread = std::thread(&FDirectoryWatcher::WatchThreadFunc, this);

	std::wstring StartMsg = L"[DirectoryWatcher] Started watching: " + WatchDirectory + L"\n";
	OutputDebugStringW(StartMsg.c_str());
	return true;
}

void FDirectoryWatcher::Stop()
{
	if (!bIsWatching) return;
	bIsWatching = false;

	// 스레드 루프 탈출을 위해 Stop 이벤트 신호 전송
	if (hStopEvent != NULL)
	{
		SetEvent(hStopEvent);
	}

	if (WatchThread.joinable())
	{
		WatchThread.join();
	}

	if (hDirectory != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hDirectory);
		hDirectory = INVALID_HANDLE_VALUE;
	}

	if (hStopEvent != NULL)
	{
		CloseHandle(hStopEvent);
		hStopEvent = NULL;
	}
}

bool FDirectoryWatcher::GetModifiedFiles(std::vector<std::wstring>& OutFiles)
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (ModifiedFiles.empty()) return false;

	// set의 내용을 vector로 옮기고 초기화
	OutFiles.assign(ModifiedFiles.begin(), ModifiedFiles.end());
	ModifiedFiles.clear();
	return true;
}

void FDirectoryWatcher::WatchThreadFunc()
{
	// 결과를 받을 버퍼 (64KB면 충분히 넉넉합니다)
	const DWORD BufferSize = 1024 * 64;
	std::vector<BYTE> Buffer(BufferSize);
	DWORD BytesReturned = 0;

	OVERLAPPED Overlapped = {};
	Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// 대기할 이벤트 2개: [0] 파일 변경, [1] 감시 종료(Stop)
	HANDLE Events[2] = { Overlapped.hEvent, hStopEvent };

	while (bIsWatching)
	{
		// 비동기(Overlapped)로 변경 감시 요청
		bool bReadSuccess = ReadDirectoryChangesW(
			hDirectory,
			Buffer.data(),
			BufferSize,
			TRUE, // 하위 폴더까지 감시 (true)
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
			NULL,
			&Overlapped,
			NULL
		);

		if (!bReadSuccess) break;

		// 이벤트가 발생할 때까지 대기
		DWORD WaitResult = WaitForMultipleObjects(2, Events, FALSE, INFINITE);

		if (WaitResult == WAIT_OBJECT_0 + 1)
		{
			// hStopEvent 신호가 들어왔으므로 스레드 정상 종료
			break;
		}

		if (WaitResult == WAIT_OBJECT_0)
		{
			// 파일 변경 이벤트 발생
			if (GetOverlappedResult(hDirectory, &Overlapped, &BytesReturned, FALSE) && BytesReturned > 0)
			{
				FILE_NOTIFY_INFORMATION* pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(Buffer.data());

				while (true)
				{
					// 파일이 추가되었거나, 수정되었거나, 이름이 변경된 경우만 캐치
					if (pNotify->Action == FILE_ACTION_ADDED ||
						pNotify->Action == FILE_ACTION_MODIFIED ||
						pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME)
					{
						std::wstring FileName(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));

						// 임시 파일 무시 (보통 .tmp 로 끝나거나 ~ 로 시작함)
						if (FileName.find(L".tmp") == std::wstring::npos && FileName.find(L"~") != 0)
						{
							std::lock_guard<std::mutex> lock(Mutex);
							ModifiedFiles.insert(FileName);
						}
					}

					// 다음 엔트리가 없으면 루프 탈출
					if (pNotify->NextEntryOffset == 0) break;
					pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(pNotify) + pNotify->NextEntryOffset);
				}
			}

			// 다음 감시를 위해 이벤트 리셋
			ResetEvent(Overlapped.hEvent);
		}
	}

	CloseHandle(Overlapped.hEvent);
}