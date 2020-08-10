// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inc/Helper/VectorSetReaders/DefaultReader.h"
#include "inc/Helper/CommonHelper.h"
#include <inc\Core\Common\PQQuantizer.h>

using namespace SPTAG;
using namespace SPTAG::Helper;

DefaultVectorReader::DefaultVectorReader(std::shared_ptr<ReaderOptions> p_options)
    : VectorSetReader(std::move(p_options))
{
    m_vectorOutput = "";
    m_metadataConentOutput = "";
    m_metadataIndexOutput = "";
}


DefaultVectorReader::~DefaultVectorReader()
{
}


ErrorCode
DefaultVectorReader::LoadFile(const std::string& p_filePaths)
{
    const auto& files = SPTAG::Helper::StrUtils::SplitString(p_filePaths, ",");
    m_vectorOutput = files[0];
    if (files.size() >= 3) {
        m_metadataConentOutput = files[1];
        m_metadataIndexOutput = files[2];
    }
    return ErrorCode::Success;
}


std::shared_ptr<VectorSet>
DefaultVectorReader::GetVectorSet() const
{
    auto ptr = f_createIO();
    if (ptr == nullptr || !ptr->Initialize(m_vectorOutput.c_str(), std::ios::binary | std::ios::in)) {
        LOG(Helper::LogLevel::LL_Error, "Failed to read file %s.\n", m_vectorOutput.c_str());
        exit(1);
    }

    SizeType row;
    DimensionType col;
    DimensionType NumSubvectors;
    SizeType KsPerSubvector;
    DimensionType DimPerSubvector;
    float*** codebooks;
    if (ptr->ReadBinary(sizeof(SizeType), (char*)&row) != sizeof(SizeType)) {
        LOG(Helper::LogLevel::LL_Error, "Failed to read VectorSet!\n");
        exit(1);
    }
    if (ptr->ReadBinary(sizeof(DimensionType), (char*)&col) != sizeof(DimensionType)) {
        LOG(Helper::LogLevel::LL_Error, "Failed to read VectorSet!\n");
        exit(1);
    }
    if (m_options->m_quantized) {
        if (ptr->ReadBinary(sizeof(DimensionType), (char*)&NumSubvectors) != sizeof(DimensionType)) {
            LOG(Helper::LogLevel::LL_Error, "Failed to read VectorSet!\n");
            exit(1);
        }

        if (ptr->ReadBinary(sizeof(SizeType), (char*)&KsPerSubvector) != sizeof(SizeType)) {
            LOG(Helper::LogLevel::LL_Error, "Failed to read VectorSet!\n");
            exit(1);
        }

        if (ptr->ReadBinary(sizeof(DimensionType), (char*)&DimPerSubvector) != sizeof(DimensionType)) {
            LOG(Helper::LogLevel::LL_Error, "Failed to read VectorSet!\n");
            exit(1);
        }
        codebooks = new float** [NumSubvectors];
        for (int i = 0; i < NumSubvectors; i++) {
            codebooks[i] = new float* [KsPerSubvector];
            for (int j = 0; j < KsPerSubvector; j++) {
                codebooks[i][j] = new float[DimPerSubvector];
                for (int k = 0; k < DimPerSubvector; k++) {
                    if (ptr->ReadBinary(sizeof(float), (char*)&(codebooks[i][j][k])) != sizeof(float)) {
                        LOG(Helper::LogLevel::LL_Error, "Failed to read VectorSet!\n");
                        exit(1);
                    }
                }
            }
        }
        SPTAG::COMMON::PQQuantizer quantizer = SPTAG::COMMON::PQQuantizer::PQQuantizer(NumSubvectors, KsPerSubvector, DimPerSubvector, codebooks);
        SPTAG::COMMON::DistanceUtils::PQQuantizer = &quantizer;

    }
    
    std::uint64_t totalRecordVectorBytes = ((std::uint64_t)GetValueTypeSize(m_options->m_inputValueType)) * row * col;
    ByteArray vectorSet = ByteArray::Alloc(totalRecordVectorBytes);
    char* vecBuf = reinterpret_cast<char*>(vectorSet.Data());

    if (ptr->ReadBinary(totalRecordVectorBytes, vecBuf) != totalRecordVectorBytes) {
        LOG(Helper::LogLevel::LL_Error, "Failed to read VectorSet!\n");
        exit(1);
    }

    return std::shared_ptr<VectorSet>(new BasicVectorSet(vectorSet,
                                                         m_options->m_inputValueType,
                                                         col,
                                                         row));
}


std::shared_ptr<MetadataSet>
DefaultVectorReader::GetMetadataSet() const
{
    if (fileexists(m_metadataIndexOutput.c_str()) && fileexists(m_metadataConentOutput.c_str()))
        return std::shared_ptr<MetadataSet>(new FileMetadataSet(m_metadataConentOutput, m_metadataIndexOutput));
    return nullptr;
}
