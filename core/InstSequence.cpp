
#include "InstSequence.hpp"
#include "Inst.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace olympia
{
    InstSequence::InstSequence(const std::vector<InstPtr> & instructions) :
        instructions_(instructions),
        successor_count_()
    {
        id_ = "";
        for (size_t i = 0; i < instructions.size(); i++)
        {
            id_ += std::to_string(instructions[i]->getOpCode());
            if (i + 1 < instructions.size())
            {
                id_ += '-';
            }
        }
    }

    void InstSequence::addSuccessor(const std::string & successor_id)
    {
        successor_count_[successor_id]++;
    }

    std::unordered_map<std::string, float> InstSequence::getTransitions()
    {
        std::unordered_map<std::string, float> result;
        uint32_t total = 0;
        for (const auto & [_, count] : successor_count_)
        {
            total += count;
        }

        for (const auto & [successor, count] : successor_count_)
        {
            result[successor] = static_cast<float>(count) / total;
        }

        return result;
    }

} // namespace olympia