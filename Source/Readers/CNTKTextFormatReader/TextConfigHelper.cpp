//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <inttypes.h>
#include <limits>
#include "TextConfigHelper.h"
#include "DataReader.h"
#include "StringUtil.h"

using std::string;
using std::wstring;
using std::pair;
using std::vector;
using std::map;

#undef max // max is defined in minwindef.h

namespace Microsoft { namespace MSR { namespace CNTK {

TextConfigHelper::TextConfigHelper(const ConfigParameters& config)
{
    if (!config.ExistsCurrent(L"input"))
    {
        RuntimeError("CNTKTextFormatReader configuration does not contain \"input\" section.");
    }

    const ConfigParameters& input = config(L"input");

    if (input.empty())
    {
        RuntimeError("CNTKTextFormatReader configuration contains an empty \"input\" section.");
    }

    string precision = config.Find("precision", "float");
    if (AreEqualIgnoreCase(precision, "double"))
    {
        m_elementType = ElementType::tdouble;
    }
    else if (AreEqualIgnoreCase(precision, "float"))
    {
        m_elementType = ElementType::tfloat;
    }
    else
    {
        RuntimeError("Not supported precision '%s'. Expected 'double' or 'float'.", precision.c_str());
    }

    StreamId id = 0;
    map<string, wstring> aliasToInputMap;
    for (const pair<string, ConfigParameters>& section : input)
    {
        ConfigParameters input = section.second;
        wstring name = msra::strfun::utf16(section.first);

        if (!input.ExistsCurrent(L"dim") || !input.ExistsCurrent(L"format"))
        {
            RuntimeError("Input section for input '%ls' does not specify all the required parameters, "
                "\"dim\" and \"format\".", name.c_str());
        }

        StreamDescriptor stream;
        stream.m_id = id++;
        stream.m_name = name;
        stream.m_sampleDimension = input(L"dim");
        string type = input(L"format");

        if (AreEqualIgnoreCase(type, "dense"))
        {
            stream.m_storageType = StorageType::dense;
        }
        else if (AreEqualIgnoreCase(type, "sparse"))
        {
            stream.m_storageType = StorageType::sparse_csc;
            if (stream.m_sampleDimension > numeric_limits<IndexType>::max())
            {
                RuntimeError("Sample dimension (%" PRIu64 ") for sparse input '%ls'"
                    " exceeds the maximum allowed value (%" PRIu64 ").\n",
                    stream.m_sampleDimension, name.c_str(), (size_t)numeric_limits<IndexType>::max());
            }
        }
        else
        {
            RuntimeError("'format' parameter must be set either to 'dense' or 'sparse'.");
        }

        // alias is optional
        if (input.ExistsCurrent(L"alias"))
        {
            stream.m_alias = input(L"alias");
            if (stream.m_alias.empty())
            {
                RuntimeError("Alias value for input '%ls' is empty.", name.c_str());
            }
        }
        else
        {
            stream.m_alias = section.first;
        }

        if (aliasToInputMap.find(stream.m_alias) != aliasToInputMap.end())
        {
            RuntimeError("Alias %s is already mapped to input %ls.",
                stream.m_alias.c_str(), aliasToInputMap[stream.m_alias].c_str());
        }
        else
        {
            aliasToInputMap[stream.m_alias] = stream.m_name;
        }

        stream.m_elementType = m_elementType;
        m_streams.push_back(stream);
    }

    m_filepath = msra::strfun::utf16(config(L"file"));

    if (config.Exists(L"randomize"))
    {
        wstring randomizeString = config.CanBeString(L"randomize") ? config(L"randomize") : wstring();
        if (!_wcsicmp(randomizeString.c_str(), L"none"))
        {
            m_randomizationWindow = randomizeNone;
        }
        else if (!_wcsicmp(randomizeString.c_str(), L"auto"))
        {
            m_randomizationWindow = randomizeAuto;
        }
        else
        {
            m_randomizationWindow = config(L"randomize");
        }
    }
    else
    {
        m_randomizationWindow = randomizeAuto;
    }

    m_skipSequenceIds = config(L"skipSequenceIds", false);
    m_maxErrors = config(L"maxErrors", 0);
    m_traceLevel = config(L"traceLevel", 0);
    m_chunkSizeBytes = config(L"chunkSizeInBytes", 32 * 1024 * 1024); // 32 MB by default
    m_chunkCacheSize = config(L"numChunksToCache", 32); // 32 * 32 MB = 1 GB of memory in total
}

}}}
