#pragma once

#include "Core/CoreTypes.h" // (필요시 포함, uint32 등)
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>
#include <windows.h>

class FDirectoryWatcher
{
public:
	FDirectoryWatcher() = default;
	~FDirectoryWatcher();

	// 감시를 시작할 디렉토리 경로 (예: L"Shaders")
	bool Start(const std::wstring& DirectoryPath);

	// 감시 스레드 종료 및 핸들 반환
	void Stop();

	// 메인 스레드에서 호출: 변경된 파일이 있다면 OutFiles에 담고 true 반환
	bool GetModifiedFiles(std::vector<std::wstring>& OutFiles);

private:
	// 백그라운드 스레드에서 실행될 실제 감시 루프
	void WatchThreadFunc();

	std::wstring WatchDirectory;

	HANDLE hDirectory = INVALID_HANDLE_VALUE;
	HANDLE hStopEvent = NULL;

	std::thread WatchThread;
	std::atomic<bool> bIsWatching{ false };

	// 멀티스레드 동기화를 위한 뮤텍스 및 변경된 파일 목록 (중복 제거용 set)
	std::mutex Mutex;
	std::unordered_set<std::wstring> ModifiedFiles;
};