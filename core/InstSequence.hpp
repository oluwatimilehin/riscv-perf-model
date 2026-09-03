
// <InstSequence.hpp>
#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <boost/json.hpp>

#include "Inst.hpp"

namespace olympia
{
    class InstSequence
    {
      public:
        InstSequence(const std::vector<InstPtr> & instructions);
        void addSuccessor(const std::size_t & successor_id);

        std::size_t getId() const { return id_; }

        const std::vector<InstPtr> & getInstructions() const { return instructions_; }

        std::unordered_map<std::size_t, float> getTransitions();

        std::size_t getNextSequence(std::mt19937 & gen);

      private:
        std::vector<InstPtr> instructions_;
        std::unordered_map<std::size_t, uint32_t> successor_count_;
        std::size_t id_;
        std::unique_ptr<std::discrete_distribution<>> distribution_;
        std::vector<std::size_t> successors_;
    };

    inline std::ostream & operator<<(std::ostream & os, const InstSequence & sequence)
    {
        os << "Sequence[" << sequence.getId() << "]";
        return os;
    }
} // namespace olympia
