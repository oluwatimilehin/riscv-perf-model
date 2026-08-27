#include "sparta/log/MessageSource.hpp"
#include "sparta/simulation/TreeNode.hpp"

#include "OlympiaAllocators.hpp"

#include "decode/MavisUnit.hpp"
#include "TraceAnalyser.hpp"

int main(int argc, char** argv)
{
    std::cout << "Welcome to the trace analyser \n";
    sparta::TreeNode treenode("trace_analyser", "trace analyser");
    olympia::OlympiaAllocators treenode_alloc(&treenode);

    olympia::MavisUnit::MavisParameters params(&treenode_alloc);
    olympia::MavisUnit mavis_unit(&treenode_alloc, &params);
    olympia::TraceAnalyser analyser(sparta::log::MessageSource::getGlobalDebug(),
                                    mavis_unit.getFacade());
    return 0;
}