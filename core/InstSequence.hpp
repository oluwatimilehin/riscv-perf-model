
// <InstSequence.hpp>
#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Inst.hpp"

namespace olympia
{
    class InstSequence
    {
      public:
        InstSequence(const std::vector<InstPtr> & instructions);
        void addSuccessor(const std::string & successor_id);

        std::string getId() const { return id_; }

        const std::vector<InstPtr> & getInstructions() const { return instructions_; }

        std::unordered_map<std::string, float> getTransitions();

      private:
        std::vector<InstPtr> instructions_;
        std::unordered_map<std::string, uint32_t> successor_count_;
        std::string id_;
    };

    inline std::ostream & operator<<(std::ostream & os, const InstSequence & sequence)
    {
        os << "Sequence[" << sequence.getId() << "]";
        return os;
    }
} // namespace olympia
