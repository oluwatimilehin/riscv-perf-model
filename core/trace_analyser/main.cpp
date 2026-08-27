#include <iostream>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"

#include "OlympiaAllocators.hpp"

#include "decode/MavisUnit.hpp"
#include "TraceAnalyser.hpp"
#include "InstSequence.hpp"

int main(int argc, char** argv)
{
    std::cout << "Welcome to the trace analyser \n";
    sparta::TreeNode treenode("trace_analyser", "trace analyser");
    sparta::Scheduler scheduler("scheduler");

    sparta::Clock clock("clock", &scheduler);

    olympia::OlympiaAllocators treenode_alloc(&treenode);
    olympia::MavisUnit::MavisParameters params(&treenode_alloc);
    olympia::MavisUnit mavis_unit(&treenode_alloc, &params);
    olympia::TraceAnalyser analyser(sparta::log::MessageSource::getGlobalDebug(),
                                    mavis_unit.getFacade(), &clock);

    const auto sequences = analyser.analyse("../traces/many_adds.json", 2, false, false);
    std::cout << "Returned sequence of length: " << sequences.size() << std::endl;
    return 0;
}