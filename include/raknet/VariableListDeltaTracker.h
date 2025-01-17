/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  Copyright (c) 2016-2018, TES3MP Team
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <cstdlib>
#include <stdint.h>
#include "DS_List.h"
#include "BitStream.h"

#ifndef __VARIABLE_LIST_DELTA_TRACKER
#define __VARIABLE_LIST_DELTA_TRACKER

namespace RakNet
{
/// Class to write a series of variables, copy the contents to memory, and return if the newly written value is different than what was last written
/// Can also encode the reads, writes, and results directly to/from a bitstream
class VariableListDeltaTracker
{
public:
    VariableListDeltaTracker();
    ~VariableListDeltaTracker();

    // Call before using a series of WriteVar
    void StartWrite();

    bool IsPastEndOfList() const { return nextWriteIndex>=variableList.Size(); }

    /// Records the passed value of the variable to memory, and returns true if the value is different from the write before that (or if it is the first write)
    /// \pre Call StartWrite() before doing the first of a series of calls to WriteVar or other functions that call WriteVar
    /// \note Variables must be of the same type, written in the same order, each time
    template <class VarType>
    bool WriteVar(const VarType &varData)
    {
        RakNet::BitStream temp;
        temp.Write(varData);
        if (nextWriteIndex>=variableList.Size())
        {
            variableList.Push(VariableLastValueNode(temp.GetData(),temp.GetNumberOfBytesUsed()));
            ++nextWriteIndex;
            return true; // Different because it's new
        }

        if (temp.GetNumberOfBytesUsed()!=variableList[nextWriteIndex].byteLength)
        {
            auto tmp = (char *) realloc(variableList[nextWriteIndex].lastData, temp.GetNumberOfBytesUsed());
            RakAssert(tmp);
            variableList[nextWriteIndex].lastData = tmp;
            variableList[nextWriteIndex].byteLength = temp.GetNumberOfBytesUsed();
            memcpy(variableList[nextWriteIndex].lastData, temp.GetData(), temp.GetNumberOfBytesUsed());
            ++nextWriteIndex;
            variableList[nextWriteIndex].isDirty = false;
            return true; // Different because the serialized size is different
        }

        if (!variableList[nextWriteIndex].isDirty)
        {
            if (memcmp(temp.GetData(), variableList[nextWriteIndex].lastData, variableList[nextWriteIndex].byteLength) == 0)
            {
                ++nextWriteIndex;
                return false; // Same because not dirty and memcmp is the same
            }
        }

        variableList[nextWriteIndex].isDirty = false;
        memcpy(variableList[nextWriteIndex].lastData, temp.GetData(), temp.GetNumberOfBytesUsed());
        ++nextWriteIndex;
        return true; // Different because dirty or memcmp was different
    }
    /// Calls WriteVar. If the variable has changed, writes true, and writes the variable. Otherwise writes false.
    template <class VarType>
    bool WriteVarToBitstream(const VarType &varData, RakNet::BitStream *bitStream)
    {
        bool wasDifferent = WriteVar(varData);
        bitStream->Write(wasDifferent);
        if (wasDifferent)
        {
            bitStream->Write(varData);
            return true;
        }
        return false;
    }
    /// Calls WriteVarToBitstream(). Additionally, adds the boolean result of WriteVar() to boolean bit array
    template <class VarType>
    bool WriteVarToBitstream(const VarType &varData, RakNet::BitStream *bitStream, unsigned char *bArray, unsigned short writeOffset)
    {
        if (WriteVarToBitstream(varData, bitStream))
        {
            BitSize_t numberOfBitsMod8 = writeOffset & 7;

            if (numberOfBitsMod8 == 0)
                bArray[writeOffset >> 3] = 0x80;
            else
                bArray[writeOffset >> 3] |= 0x80 >> (numberOfBitsMod8); // Set the bit to 1

            return true;
        }
        else
        {
            if ((writeOffset & 7) == 0)
                bArray[writeOffset >> 3] = 0;

            return false;
        }
    }

    /// Paired with a call to WriteVarToBitstream(), will read a variable if it had changed. Otherwise the values remains the same.
    template <class VarType>
    static bool ReadVarFromBitstream(VarType &varData, RakNet::BitStream *bitStream)
    {
        bool wasWritten;
        if (!bitStream->Read(wasWritten))
            return false;
        if (wasWritten)
        {
            if (!bitStream->Read(varData))
                return false;
        }
        return wasWritten;
    }

    /// Variables flagged dirty will cause WriteVar() to return true, even if the variable had not otherwise changed
    /// This updates all the variables in the list, where in each index \a varsWritten is true, so will the variable at the corresponding index be flagged dirty
    void FlagDirtyFromBitArray(const unsigned char *bArray);

    /// \internal
    struct VariableLastValueNode
    {
        VariableLastValueNode();
        VariableLastValueNode(const unsigned char *data, size_t _byteLength);
        ~VariableLastValueNode() = default;
        char *lastData;
        size_t byteLength;
        bool isDirty;
    };

protected:
    /// \internal
    DataStructures::List<VariableLastValueNode> variableList;
    /// \internal
    unsigned int nextWriteIndex;
};


}

#endif
