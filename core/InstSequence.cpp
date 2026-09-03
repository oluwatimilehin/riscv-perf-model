
#include "InstSequence.hpp"
#include "Inst.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>
#include <unordered_map>

namespace olympia
{
    InstSequence::InstSequence(const std::vector<InstPtr> & instructions) :
        instructions_(instructions),
        successor_count_(),
        distribution_(std::make_unique<std::discrete_distribution<>>())
    {
        std::ostringstream oss;
        for (size_t i = 0; i < instructions.size(); i++)
        {
            oss << instructions[i]->hash();
            if (i + 1 < instructions.size())
            {
                oss << "-";
            }
        }

        id_ = std::hash<std::string>{}(oss.str());
    }

    void InstSequence::addSuccessor(const std::size_t & successor_id)
    {
        if (distribution_)
        {
            distribution_.reset();
        }
        successor_count_[successor_id]++;
    }

    std::unordered_map<std::size_t, float> InstSequence::getTransitions()
    {
        std::unordered_map<std::size_t, float> result;
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

    std::size_t InstSequence::getNextSequence(std::mt19937 & gen)
    {
        if (!distribution_)
        {
            successors_.clear();
            std::vector<float> weights;
            const auto & transitions = getTransitions();
            successors_.reserve(transitions.size());
            weights.reserve(transitions.size());

            for (const auto & [sequence, weight] : transitions)
            {
                successors_.push_back(sequence);
                weights.push_back(weight);
            }

            distribution_ =
                std::make_unique<std::discrete_distribution<>>(weights.begin(), weights.end());
        }

        return successors_[(*distribution_)(gen)];
    }

} // namespace olympia