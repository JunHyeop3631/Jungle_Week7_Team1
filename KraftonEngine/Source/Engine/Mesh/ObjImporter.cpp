#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Engine/Platform/Paths.h"
#include "Mesh/ObjManager.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <charconv>
#include <chrono>

const FVector FallbackColor3 = FVector(1.0f, 0.0f, 1.0f);
const FVector4 FallbackColor4 = FVector4(1.0f, 0.0f, 1.0f, 1.0f);

struct FVertexKey {
    uint32 p, t, n;
    bool operator==(const FVertexKey& Other) const {
        return p == Other.p && t == Other.t && n == Other.n;
    }
};

namespace std {
template<>
struct hash<FVertexKey>
{
    size_t operator()(const FVertexKey& Key) const noexcept
    {
        return ((size_t)Key.p) ^ (((size_t)Key.t) << 8) ^ (((size_t)Key.n) << 16);
    }
};
}

struct FStringParser
{
	// Delimiter로 구분된 다음 토큰을 추출하고, InOutView에서 해당 토큰과 구분자 제거
	static std::string_view GetNextToken(std::string_view& InOutView, char Delimiter = ' ')
	{
		size_t DelimiterPosition = InOutView.find(Delimiter);
		std::string_view Token = InOutView.substr(0, DelimiterPosition); // [0, DelimiterPosition) 범위의 토큰 추출
		if (DelimiterPosition != std::string_view::npos)
		{
			InOutView.remove_prefix(DelimiterPosition + 1); // 토큰과 구분자 제거
		}
		else
		{
			InOutView = std::string_view();
		}
		return Token;
	}

	// 다수의 공백을 구분자로 사용하여 다음 토큰을 추출하고, InOutView에서 해당 토큰과 앞의 공백 제거
	static std::string_view GetNextWhitespaceToken(std::string_view& InOutView)
	{
		size_t Start = InOutView.find_first_not_of(" \t");
		if (Start == std::string_view::npos)
		{
			InOutView = std::string_view();
			return std::string_view();
		}
		InOutView.remove_prefix(Start); // 유효한 문자 앞의 공백 제거

		size_t End = InOutView.find_first_of(" \t");
		std::string_view Token = InOutView.substr(0, End); // 공백 이전까지의 토큰 추출

		if (End != std::string_view::npos)
		{
			InOutView.remove_prefix(End);
		}
		else
		{
			InOutView = std::string_view();
		}
		return Token;
	}

	// InOutView의 왼쪽 끝에 있는 공백 제거
	static void TrimLeft(std::string_view& InOutView)
	{
		size_t Start = InOutView.find_first_not_of(" \t");
		if (Start != std::string_view::npos)
		{
			InOutView.remove_prefix(Start);  // 유효한 문자 앞의 공백 제거
		}
		else
		{
			InOutView = std::string_view();
		}
	}

	static bool ParseInt(std::string_view Str, int& OutValue)
	{
		if (Str.empty()) return false;
		std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
		return result.ec == std::errc();
	}

	static bool ParseFloat(std::string_view Str, float& OutValue)
	{
		if (Str.empty()) return false;
		std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
		return result.ec == std::errc();
	}
};

struct FRawFaceVertex
{
    int32 PosIndex = -1;
    int32 UVIndex = -1;
    int32 NormalIndex = -1;
};

FRawFaceVertex ParseSingleFaceVertex(std::string_view FaceToken)
{
    FRawFaceVertex Result;

    // 첫 번째 토큰: Position
    std::string_view PosStr = FStringParser::GetNextToken(FaceToken, '/');
    FStringParser::ParseInt(PosStr, Result.PosIndex);

    // 두 번째 토큰: UV (있을 수도, 비어있을 수도 있음)
    if (!FaceToken.empty())
    {
        std::string_view UVStr = FStringParser::GetNextToken(FaceToken, '/');
        if (!UVStr.empty())
        {
            FStringParser::ParseInt(UVStr, Result.UVIndex);
        }
    }

    // 세 번째 토큰: Normal
    if (!FaceToken.empty())
    {
        std::string_view NormalStr = FStringParser::GetNextToken(FaceToken, '/');
        FStringParser::ParseInt(NormalStr, Result.NormalIndex);
    }

    return Result;
}

bool FObjImporter::ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo)
{
	OutObjInfo = FObjInfo();

	std::ifstream File(FPaths::ToWide(ObjFilePath), std::ios::binary | std::ios::ate);
	if (!File.is_open())
	{
		UE_LOG("Failed to open OBJ file: %s", ObjFilePath.c_str());
		return false;
	}

	size_t FileSize = static_cast<size_t>(File.tellg());
	File.seekg(0, std::ios::beg);
	TArray<char> Buffer(FileSize);
	if (!File.read(Buffer.data(), FileSize))
	{
		UE_LOG("Failed to read OBJ file: %s", ObjFilePath.c_str());
		return false;
	}

	std::string_view FileView(Buffer.data(), Buffer.size());

	// UTF-8 BOM 스킵
	if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
	{
		FileView.remove_prefix(3);
	}

	TArray<FRawFaceVertex> FaceVertices;
	FaceVertices.reserve(6); // Heuristic

	while (!FileView.empty())
	{
		std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

		// CRLF 제거
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.remove_suffix(1);
		}

		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::string_view Prefix = FStringParser::GetNextToken(Line);

		if (Prefix == "v")
		{
			FVector Position;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Z);
			OutObjInfo.Positions.emplace_back(Position);
		}
		else if (Prefix == "vt")
		{
			FVector2 UV;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.U);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.V);
			OutObjInfo.UVs.emplace_back(UV);
		}
		else if (Prefix == "vn")
		{
			FVector Normal;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Z);
			OutObjInfo.Normals.emplace_back(Normal);
		}
		else if (Prefix == "f")
		{
			// default material section 추가 (usemtl이 없이 f가 먼저 나오는 경우)
			if (OutObjInfo.Sections.empty())
			{
				FStaticMeshSection DefaultSection;
				DefaultSection.MaterialSlotName = "None";
				DefaultSection.FirstIndex = 0;
				DefaultSection.NumTriangles = 0;
				OutObjInfo.Sections.emplace_back(DefaultSection);
			}

			while (!Line.empty())
			{
				std::string_view FaceToken = FStringParser::GetNextToken(Line, ' ');
				if (!FaceToken.empty())
				{
					FaceVertices.push_back(ParseSingleFaceVertex(FaceToken));
				}
			}

			if (FaceVertices.size() < 3)
			{
				UE_LOG("Face with less than 3 vertices");
				continue;
			}

			// Fan triangulation
			for (size_t i = 1; i + 1 < FaceVertices.size(); ++i)
			{
				const std::array<FRawFaceVertex, 3> TriangleVerts = { FaceVertices[0], FaceVertices[i], FaceVertices[i + 1] };
				for (int j = 0; j < 3; ++j)
				{
					constexpr int32 InvalidIndex = -1;
					OutObjInfo.PosIndices.emplace_back(TriangleVerts[j].PosIndex - 1);
					OutObjInfo.UVIndices.emplace_back(TriangleVerts[j].UVIndex > 0 ? TriangleVerts[j].UVIndex - 1 : InvalidIndex);
					OutObjInfo.NormalIndices.emplace_back(TriangleVerts[j].NormalIndex > 0 ? TriangleVerts[j].NormalIndex - 1 : InvalidIndex);
				}
			}
			FaceVertices.clear();
		}
		else
		{
			if (Prefix == "mtllib")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);
				OutObjInfo.MaterialLibraryFilePath = FPaths::ResolveAssetPath(ObjFilePath, std::string(Line));
				UE_LOG("Found material library: %s", OutObjInfo.MaterialLibraryFilePath.c_str());
			}
			else if (Prefix == "usemtl")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);

				if (!OutObjInfo.Sections.empty())
				{
					OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
				}
				FStaticMeshSection Section;
				Section.MaterialSlotName = std::string(Line);
				if (Section.MaterialSlotName.empty())
				{
					Section.MaterialSlotName = "None";
				}
				Section.FirstIndex = static_cast<uint32>(OutObjInfo.PosIndices.size());
				OutObjInfo.Sections.emplace_back(Section);
			}
			else if (Prefix == "o")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);

				OutObjInfo.ObjectName = std::string(Line);
			}
		}
	}

	if (!OutObjInfo.Sections.empty())
	{
		OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
	}

	if (OutObjInfo.UVs.empty())
	{
		OutObjInfo.UVs.emplace_back(FVector2{ 0.0f, 0.0f });
	}

	return true;
}

bool FObjImporter::ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMtlInfos)
{
	OutMtlInfos.clear();
	std::ifstream File(FPaths::ToWide(MtlFilePath), std::ios::binary | std::ios::ate);

	if (!File.is_open())
	{
		UE_LOG("Failed to open MTL file: %s", MtlFilePath.c_str());
		return false;
	}

	size_t FileSize = static_cast<size_t>(File.tellg());
	File.seekg(0, std::ios::beg);
	TArray<char> Buffer(FileSize);
	if (!File.read(Buffer.data(), FileSize))
	{
		UE_LOG("Failed to read MTL file: %s", MtlFilePath.c_str());
		return false;
	}

	std::string_view FileView(Buffer.data(), Buffer.size());

	// UTF-8 BOM 스킵
	if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
	{
		FileView.remove_prefix(3);
	}

	while (!FileView.empty())
	{
		std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

		// CRLF 제거
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.remove_suffix(1);
		}

		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::string_view Prefix = FStringParser::GetNextWhitespaceToken(Line);

		if (Prefix == "newmtl")
		{
			FObjMaterialInfo MaterialInfo;
			FStringParser::TrimLeft(Line);
			MaterialInfo.MaterialSlotName = std::string(Line);
			MaterialInfo.Kd = FallbackColor3;
			OutMtlInfos.emplace_back(MaterialInfo);
		}
		else if (Prefix == "Ka")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.Z);
		}
		else if (Prefix == "Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Z);
		}
		else if (Prefix == "Ks")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Z);
		}
		else if (Prefix == "Ns")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ns);
		}
		else if (Prefix == "map_Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}

			std::string TextureFileName;

			// 토큰 단위로 옵션들을 건너뜁니다.
			while (!Line.empty())
			{
				// 파일명에 공백이 포함될 수 있으므로, 현재 Line의 상태를 백업해둡니다.
				std::string_view LineBeforeToken = Line;
				std::string_view Token = FStringParser::GetNextWhitespaceToken(Line);

				if (Token.empty()) break;

				// 토큰이 '-'로 시작하면 옵션 플래그인지 확인합니다.
				if (Token[0] == '-')
				{
					int32 ArgsToSkip = 0;

					// 1. 3개의 인자를 받는 옵션 (Vector)
					if (Token == "-s" || Token == "-o" || Token == "-t")
					{
						ArgsToSkip = 3;
					}
					// 2. 2개의 인자를 받는 옵션
					else if (Token == "-mm")
					{
						ArgsToSkip = 2;
					}
					// 3. 1개의 인자를 받는 옵션 (Float, String, Bool)
					else if (Token == "-bm" || Token == "-boost" || Token == "-texres" ||
							 Token == "-blendu" || Token == "-blendv" || Token == "-clamp" ||
							 Token == "-cc" || Token == "-imfchan")
					{
						ArgsToSkip = 1;
					}

					// 파악된 옵션의 인자 개수만큼 다음 토큰들을 무시합니다.
					for (int32 i = 0; i < ArgsToSkip; ++i)
					{
						FStringParser::GetNextWhitespaceToken(Line);
					}
				}
				else
				{
					// '-'로 시작하지 않는 첫 번째 토큰을 만났다면, 이것이 파일명의 시작입니다!
					// 파일명 내부에 띄어쓰기가 있을 수 있으므로 토큰을 뽑기 전의 전체 라인을 가져옵니다.
					FStringParser::TrimLeft(LineBeforeToken);
					TextureFileName = FString(LineBeforeToken);
					break;
				}
			}

			// 문자열 끝에 남아있을지 모르는 쓸데없는 공백이나 탭을 정리합니다. (RTrim)
			size_t LastNonSpace = TextureFileName.find_last_not_of(" \t");
			if (LastNonSpace != FString::npos)
			{
				TextureFileName.erase(LastNonSpace + 1);
			}

			// 최종적으로 추출된 파일명 할당
			if (!TextureFileName.empty())
			{
				OutMtlInfos.back().map_Kd = FPaths::ResolveAssetPath(MtlFilePath, TextureFileName);
			}
		}
		else if (Prefix == "map_Bump" || Prefix == "norm")
		{
			if (OutMtlInfos.empty()) continue;

			std::string TextureFileName;
			while (!Line.empty())
			{
				std::string_view LineBeforeToken = Line;
				std::string_view Token = FStringParser::GetNextWhitespaceToken(Line);
				if (Token.empty()) break;

				if (Token[0] == '-')
				{
					int32 ArgsToSkip = 0;
					if (Token == "-s" || Token == "-o" || Token == "-t") ArgsToSkip = 3;
					else if (Token == "-mm") ArgsToSkip = 2;
					else if (Token == "-bm" || Token == "-boost" || Token == "-texres" ||
						Token == "-blendu" || Token == "-blendv" || Token == "-clamp" ||
						Token == "-cc" || Token == "-imfchan") ArgsToSkip = 1;

					for (int32 i = 0; i < ArgsToSkip; ++i) FStringParser::GetNextWhitespaceToken(Line);
				}
				else
				{
					FStringParser::TrimLeft(LineBeforeToken);
					TextureFileName = FString(LineBeforeToken);
					break;
				}
			}

			size_t LastNonSpace = TextureFileName.find_last_not_of(" \t");
			if (LastNonSpace != FString::npos) TextureFileName.erase(LastNonSpace + 1);

			if (!TextureFileName.empty())
			{
				OutMtlInfos.back().map_Bump = FPaths::ResolveAssetPath(MtlFilePath, TextureFileName);
			}
		}
	}

	return true;
}

FVector FObjImporter::RemapPosition(const FVector& ObjPos, EForwardAxis Axis)
{
 // OBJ 원본 좌표계를 Unreal 축(+X Forward, +Y Right, +Z Up)으로 투영한다.
	// ForwardAxis는 "원본 데이터에서 Forward가 어느 축인지"를 의미한다.
	FVector SourceForward;
	switch (Axis)
	{
	case EForwardAxis::X:    SourceForward = FVector(1.0f, 0.0f, 0.0f); break;
	case EForwardAxis::NegX: SourceForward = FVector(-1.0f, 0.0f, 0.0f); break;
	case EForwardAxis::Y:    SourceForward = FVector(0.0f, 1.0f, 0.0f); break;
	case EForwardAxis::NegY: SourceForward = FVector(0.0f, -1.0f, 0.0f); break;
	case EForwardAxis::Z:    SourceForward = FVector(0.0f, 0.0f, 1.0f); break;
	case EForwardAxis::NegZ: SourceForward = FVector(0.0f, 0.0f, -1.0f); break;
	default:                 SourceForward = FVector(0.0f, -1.0f, 0.0f); break;
	}

	// 기본은 Blender와 동일하게 Z-up을 가정.
	// 단, Forward가 ±Z면 보조 Up을 Y로 바꿔 직교 기저를 만든다.
	FVector SourceUp(0.0f, 0.0f, 1.0f);
	if (std::abs(SourceForward.Dot(SourceUp)) > 0.9999f)
	{
		SourceUp = FVector(0.0f, 1.0f, 0.0f);
	}

	const FVector SourceRight = FVector::Cross(SourceUp, SourceForward).Normalized();
	const FVector SourceOrthoUp = FVector::Cross(SourceForward, SourceRight).Normalized();

	return FVector(
		ObjPos.Dot(SourceForward),
		ObjPos.Dot(SourceRight),
		ObjPos.Dot(SourceOrthoUp));
}

bool FObjImporter::Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	OutMesh = FStaticMesh();
	OutMaterials.clear();

	// Phase 1: usemtl 등장 순서를 기반으로 FStaticMaterial 배열 및 인덱스 맵 생성
	TArray<FString> OrderedMaterialSlots;
	bool bHasNoneSlot = false;

	// OBJ의 Sections(usemtl) 등장 순서대로 고유 슬롯 수집
	for (const FStaticMeshSection& Section : ObjInfo.Sections)
	{
		const FString& CurrentSlotName = Section.MaterialSlotName;

		if (CurrentSlotName == "None")
		{
			bHasNoneSlot = true;
			continue;
		}

		// 기존에 수집된 슬롯과 중복되지 않는 경우에만 추가
		if (std::find(OrderedMaterialSlots.begin(), OrderedMaterialSlots.end(), CurrentSlotName) == OrderedMaterialSlots.end())
		{
			OrderedMaterialSlots.push_back(CurrentSlotName);
		}
	}

	// 수집된 순서대로 머티리얼 생성 및 인덱스 매핑
	for (const FString& TargetSlotName : OrderedMaterialSlots)
	{
		// 슬롯 이름과 일치하는 파싱된 머티리얼 데이터 선형 탐색
		const FObjMaterialInfo* MatchedMaterial = nullptr;
		auto It = std::find_if(MtlInfos.begin(), MtlInfos.end(),
			[&TargetSlotName](const FObjMaterialInfo& Mat) {
				return Mat.MaterialSlotName == TargetSlotName;
			});

		if (It != MtlInfos.end())
		{
			MatchedMaterial = &(*It);
			// 섹션 머티리얼 슬롯 이름과 일치하는 머티리얼 이름이 MTL 파일에서 발견된 경우, 해당 머티리얼 로드 또는 생성
			UE_LOG("Importer TargetSlotName: %s;", TargetSlotName.c_str());
         UMaterialInterface* LoadedMaterial = FObjManager::GetOrLoadMaterial(TargetSlotName);
			UMaterial* MaterialObject = Cast<UMaterial>(LoadedMaterial);
			if (!MaterialObject)
			{
				continue;
			}

			// 머티리얼 객체가 새로 생성된 경우에만 속성 설정 (캐시에서 로드된 경우 이미 설정되어 있다고 가정)
			if (MaterialObject->PathFileName.empty())
			{
				MaterialObject->PathFileName = TargetSlotName;

				if (!MatchedMaterial->map_Kd.empty())
				{
					MaterialObject->DiffuseTextureFilePath = MatchedMaterial->map_Kd;
					MaterialObject->DiffuseColor = { 1,1,1,1 }; // 텍스처 색 유지용
				}
				else
				{
					MaterialObject->DiffuseColor = {
						MatchedMaterial->Kd.X,
						MatchedMaterial->Kd.Y,
						MatchedMaterial->Kd.Z,
						1.0f
					};
				}

				// 항상 공통
				MaterialObject->AmbientColor = {
					MatchedMaterial->Ka.X,
					MatchedMaterial->Ka.Y,
					MatchedMaterial->Ka.Z,
					1.0f
				};

				MaterialObject->SpecularColor = {
					MatchedMaterial->Ks.X,
					MatchedMaterial->Ks.Y,
					MatchedMaterial->Ks.Z,
					1.0f
				};

				MaterialObject->SpecularExponent = MatchedMaterial->Ns;

				if (!MatchedMaterial->map_Bump.empty())
				{
					MaterialObject->NormalTextureFilePath = MatchedMaterial->map_Bump;
				}
			}

			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewStaticMaterial;
			NewStaticMaterial.MaterialInterface = MaterialObject;
			NewStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewStaticMaterial);
		}
		else // Material Slot이 MTL 파일에 정의되어 있지 않은 경우
		{
          UMaterialInterface* LoadedDefaultMaterial = FObjManager::GetOrLoadMaterial("None");
			UMaterial* DefaultMaterialObject = Cast<UMaterial>(LoadedDefaultMaterial);
			if (!DefaultMaterialObject)
			{
				continue;
			}
			if (DefaultMaterialObject->PathFileName.empty())
			{
				DefaultMaterialObject->PathFileName = "None";
				DefaultMaterialObject->DiffuseColor = FallbackColor4;
			}

			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewEmptyStaticMaterial;
			NewEmptyStaticMaterial.MaterialInterface = DefaultMaterialObject;
			NewEmptyStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewEmptyStaticMaterial);
		}
	}

	// "None" 슬롯이 존재했다면 맨 마지막에 배치
	if (bHasNoneSlot)
	{
      UMaterialInterface* LoadedDefaultMaterial = FObjManager::GetOrLoadMaterial("None");
		UMaterial* DefaultMaterialObject = Cast<UMaterial>(LoadedDefaultMaterial);
		if (!DefaultMaterialObject)
		{
			return false;
		}
		if (DefaultMaterialObject->PathFileName.empty())
		{
			DefaultMaterialObject->PathFileName = "None";
			DefaultMaterialObject->DiffuseColor = FallbackColor4;
		}

		FStaticMaterial NewDefaultStaticMaterial;
		NewDefaultStaticMaterial.MaterialInterface = DefaultMaterialObject;
		NewDefaultStaticMaterial.MaterialSlotName = "None";

		OutMaterials.push_back(NewDefaultStaticMaterial);
	}

    // Phase 2: 파편화된 섹션들의 면(Face)을 머티리얼 인덱스 기준으로 재그룹화
	TArray<TArray<uint32>> FacesPerMaterial;
	FacesPerMaterial.resize(OutMaterials.size());

	for (const FStaticMeshSection& RawSection : ObjInfo.Sections)
	{
		// 섹션의 머티리얼 슬롯 이름과 일치하는 OutMaterials 배열의 인덱스 찾기
		auto It = std::find_if(OutMaterials.begin(), OutMaterials.end(),
			[&RawSection](const FStaticMaterial& Mat) {
				return Mat.MaterialSlotName == RawSection.MaterialSlotName;
			});

		size_t MaterialIndex = 0;
		if (It != OutMaterials.end())
		{
			MaterialIndex = std::distance(OutMaterials.begin(), It);
		}
		else
		{
			// "None" 슬롯이 없고 매칭되는 슬롯도 없는 경우, 기본 머티리얼로 할당
			MaterialIndex = OutMaterials.size() - 1; // "None" 슬롯이 마지막에 배치되어 있다고 가정
			UE_LOG("Warning: Material slot '%s' not found. Assigning to Default slot.", RawSection.MaterialSlotName.c_str());
		}

		for (uint32 i = 0; i < RawSection.NumTriangles; ++i)
		{
			uint32 FaceStartIndex = RawSection.FirstIndex + (i * 3);
			FacesPerMaterial[MaterialIndex].push_back(FaceStartIndex);
		}
	}

    // Phase 3: 재그룹화된 면 데이터를 기반으로 최종 정점/인덱스 버퍼 구축
	TMap<FVertexKey, uint32> VertexMap;

	for (size_t MaterialIndex = 0; MaterialIndex < OutMaterials.size(); ++MaterialIndex)
	{
		const TArray<uint32>& FaceStarts = FacesPerMaterial[MaterialIndex];
		if (FaceStarts.empty()) continue;

		FStaticMeshSection NewSection;
		NewSection.MaterialIndex = static_cast<int32>(MaterialIndex);
		NewSection.MaterialSlotName = OutMaterials[MaterialIndex].MaterialSlotName;
		NewSection.FirstIndex = static_cast<uint32>(OutMesh.Indices.size());
		NewSection.NumTriangles = static_cast<uint32>(FaceStarts.size());

		for (uint32 FaceStartIndex : FaceStarts)
		{
			uint32 TriangleIndices[3];

			// (P1 - P0) X (P2 - P0) 후 정규화
			FVector P0 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex]];
			FVector P1 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 1]];
			FVector P2 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 2]];

			FVector Edge1 = P1 - P0;
			FVector Edge2 = P2 - P0;
			FVector FaceNormal = Edge1.Cross(Edge2).Normalized();
			if (Options.WindingOrder == EWindingOrder::CCW_to_CW)
			{
				FaceNormal = FaceNormal * -1.0f;
			}

			for (int j = 0; j < 3; ++j)
			{
				size_t CurrentIndex = FaceStartIndex + j;
				FVertexKey Key = {
					ObjInfo.PosIndices[CurrentIndex],
					ObjInfo.UVIndices[CurrentIndex],
					ObjInfo.NormalIndices[CurrentIndex]
				};

				if (auto It = VertexMap.find(Key); It != VertexMap.end())
				{
					// 이미 생성된 정점이 있다면 인덱스 재사용
					TriangleIndices[j] = It->second;
				}
				else
				{
					// 새로운 정점 생성
					FNormalVertex NewVertex;

					// 축 리맵 + 스케일 적용
					NewVertex.pos = RemapPosition(ObjInfo.Positions[Key.p], Options.ForwardAxis) * Options.Scale;

					// Normal 리맵
					if (Key.n == -1)
					{
						NewVertex.normal = RemapPosition(FaceNormal, Options.ForwardAxis).Normalized();
					}
					else
					{
						NewVertex.normal = RemapPosition(ObjInfo.Normals[Key.n], Options.ForwardAxis).Normalized();
					}

					// UV 예외 처리
					if (Key.t == -1)
					{
						NewVertex.tex = { 0.0f, 0.0f };
					}
					else
					{
						NewVertex.tex = ObjInfo.UVs[Key.t];
						// UV 변환 (left-bottom -> left-top)
						NewVertex.tex.V = 1.0f - NewVertex.tex.V;
					}

					NewVertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

					uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
					OutMesh.Vertices.push_back(NewVertex);

					VertexMap[Key] = NewIndex;
					TriangleIndices[j] = NewIndex;
				}
			}

			// 와인딩 오더 처리
			OutMesh.Indices.push_back(TriangleIndices[0]);
			if (Options.WindingOrder == EWindingOrder::CCW_to_CW)
			{
				OutMesh.Indices.push_back(TriangleIndices[2]);
				OutMesh.Indices.push_back(TriangleIndices[1]);
			}
			else
			{
				OutMesh.Indices.push_back(TriangleIndices[1]);
				OutMesh.Indices.push_back(TriangleIndices[2]);
			}
		}

		OutMesh.Sections.push_back(NewSection);
	}

	// =================================================================
	// Tangent & Bitangent 계산 (Gram-Schmidt 직교화 포함)
	// =================================================================
	size_t VertexCount = OutMesh.Vertices.size();
	TArray<FVector> TempTangents(VertexCount, FVector(0.0f, 0.0f, 0.0f));
	TArray<FVector> TempBitangents(VertexCount, FVector(0.0f, 0.0f, 0.0f));

	// 1. 모든 삼각형(Triangle)을 순회하며 Face Tangent를 계산하여 누적
	for (size_t i = 0; i < OutMesh.Indices.size(); i += 3)
	{
		uint32 i0 = OutMesh.Indices[i];
		uint32 i1 = OutMesh.Indices[i + 1];
		uint32 i2 = OutMesh.Indices[i + 2];

		const FVector& v0 = OutMesh.Vertices[i0].pos;
		const FVector& v1 = OutMesh.Vertices[i1].pos;
		const FVector& v2 = OutMesh.Vertices[i2].pos;

		const FVector2& uv0 = OutMesh.Vertices[i0].tex;
		const FVector2& uv1 = OutMesh.Vertices[i1].tex;
		const FVector2& uv2 = OutMesh.Vertices[i2].tex;

		// 위치와 UV의 델타(Delta) 값 계산 (Vector.h의 operator 오버로딩 활용)
		FVector Edge1 = v1 - v0;
		FVector Edge2 = v2 - v0;
		FVector2 DeltaUV1 = uv1 - uv0;
		FVector2 DeltaUV2 = uv2 - uv0;

		// X, Y 멤버 접근 보장 (FVector2의 union 구조체 활용)
		float f = 1.0f / (DeltaUV1.X * DeltaUV2.Y - DeltaUV2.X * DeltaUV1.Y);

		FVector Tangent;
		Tangent.X = f * (DeltaUV2.Y * Edge1.X - DeltaUV1.Y * Edge2.X);
		Tangent.Y = f * (DeltaUV2.Y * Edge1.Y - DeltaUV1.Y * Edge2.Y);
		Tangent.Z = f * (DeltaUV2.Y * Edge1.Z - DeltaUV1.Y * Edge2.Z);

		FVector Bitangent;
		Bitangent.X = f * (-DeltaUV2.X * Edge1.X + DeltaUV1.X * Edge2.X);
		Bitangent.Y = f * (-DeltaUV2.X * Edge1.Y + DeltaUV1.X * Edge2.Y);
		Bitangent.Z = f * (-DeltaUV2.X * Edge1.Z + DeltaUV1.X * Edge2.Z);

		// 해당 삼각형을 공유하는 정점들에 누적(Accumulate)
		TempTangents[i0] = TempTangents[i0] + Tangent;
		TempTangents[i1] = TempTangents[i1] + Tangent;
		TempTangents[i2] = TempTangents[i2] + Tangent;

		TempBitangents[i0] = TempBitangents[i0] + Bitangent;
		TempBitangents[i1] = TempBitangents[i1] + Bitangent;
		TempBitangents[i2] = TempBitangents[i2] + Bitangent;
	}

	// 2. 각 정점별로 직교화(Orthogonalize) 및 Handedness(W값) 결정
	for (size_t i = 0; i < VertexCount; ++i)
	{
		const FVector& n = OutMesh.Vertices[i].normal;
		const FVector& t = TempTangents[i];
		const FVector& b = TempBitangents[i];

		// Gram-Schmidt 직교화: t = normalize(t - n * dot(n, t))
		float dotNT = n.Dot(t);                     // 엔진의 멤버 함수 Dot 활용
		FVector orthogonalT = t - n * dotNT;        // 엔진의 operator- 및 operator* 활용
		orthogonalT.Normalize();                    // 엔진의 멤버 함수 Normalize 활용

		// 뒤집힘(Handedness) 판별: N과 T의 외적 결과와 B의 내적을 확인
		FVector crossNT = FVector::Cross(n, orthogonalT);  // 엔진의 static Cross 함수 활용
		float handedness = (crossNT.Dot(b) < 0.0f) ? -1.0f : 1.0f;

		// 최종 계산된 Tangent와 W값을 렌더링용 정점 구조체에 저장
		OutMesh.Vertices[i].tangent = FVector4(orthogonalT.X, orthogonalT.Y, orthogonalT.Z, handedness);
	}

    return true;
}

bool FObjImporter::Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	return Import(ObjFilePath, FImportOptions::Default(), OutMesh, OutMaterials);
}

bool FObjImporter::Import(const FString& ObjFilePath, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	auto StartTime = std::chrono::high_resolution_clock::now();

	OutMaterials.clear();

	FObjInfo ObjInfo;
	if (!FObjImporter::ParseObj(ObjFilePath, ObjInfo))
	{
		UE_LOG("ParseObj failed for: %s", ObjFilePath.c_str());
		return false;
	}

	TArray<FObjMaterialInfo> ParsedMtlInfos;
	if (!ObjInfo.MaterialLibraryFilePath.empty()) {
		if (!FObjImporter::ParseMtl(ObjInfo.MaterialLibraryFilePath, ParsedMtlInfos))
		{
			UE_LOG("ParseMtl failed for: %s", ObjInfo.MaterialLibraryFilePath.c_str());
			ObjInfo.MaterialLibraryFilePath.clear();
			ParsedMtlInfos.clear();
		}
	}

	if (!FObjImporter::Convert(ObjInfo, ParsedMtlInfos, Options, OutMesh, OutMaterials)){
		UE_LOG("Convert failed for: %s", ObjFilePath.c_str());
		return false;
	}
	OutMesh.PathFileName = ObjFilePath;

	auto EndTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> Duration = EndTime - StartTime;
	UE_LOG("OBJ Imported successfully. File: %s. Time taken: %.4f seconds", ObjFilePath.c_str(), Duration.count());

	return true;
}
