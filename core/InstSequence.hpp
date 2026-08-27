
// <InstSequence.hpp>
#pragma once

#include <cstdint>
#include <ostream>
#include <random>
#include <string>
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
        void addSuccessor(const std::string & successor_id);

        std::string getId() const { return id_; }

        const std::vector<InstPtr> & getInstructions() const { return instructions_; }

        std::unordered_map<std::string, float> getTransitions();

        std::string getNextSequence(std::mt19937 & gen);

      private:
        std::vector<InstPtr> instructions_;
        std::unordered_map<std::string, uint32_t> successor_count_;
        std::string id_;
        std::unique_ptr<std::discrete_distribution<>> distribution_;
        std::vector<std::string> successors_;
    };

    inline std::ostream & operator<<(std::ostream & os, const InstSequence & sequence)
    {
        os << "Sequence[" << sequence.getId() << "]";
        return os;
    }
} // namespace olympia
