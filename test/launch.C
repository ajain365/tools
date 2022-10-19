#include <iostream>
#include <cstring>
#include <vector>
#include <queue>
#include "BPatch.h"
#include "Graph.h"
#include "slicing.h"
#include "dyn_regs.h"
#include "CFG.h"
#include "CodeObject.h"
#include "InstructionDecoder.h"
#include "entryIDs.h"
#include <elf.h>

namespace dp = Dyninst::ParseAPI; 
namespace ds = Dyninst::SymtabAPI;
namespace di = Dyninst::InstructionAPI;

namespace {
} // end anonymous namespace

int main( int argc, char* argv[] )
{
    std::string execName = "libopen-pal.so";
    

    ds::Symtab* obj = nullptr;
    bool success = ds::Symtab::openFile( obj, execName ); 
    
    if ( ! success ) {
        std::cerr << "Could not open file: " << execName << std::endl;
        return 1;
    }

    std::cout << "Processing File: " << execName << std::endl;

    // traverse call graph
    auto sts = new dp::SymtabCodeSource( const_cast<char*>( execName.c_str() ) );
    auto co = new dp::CodeObject( sts );
    co->parse();
    auto funcList = co->funcs();

    int targetAddress = 0x784d8;
    for ( auto const* f: funcList ) {
        for ( auto blk: f->blocks() ) {
            if ( blk->start() <= targetAddress && blk->end() > targetAddress ) {
                auto reg = obj->findEnclosingRegion( blk->start() );
                if ( ! reg ) {
                    return 0;
                }
                auto bufStart = (const char*) reg->getPtrToRawData() + blk->start() - reg->getMemOffset();
                auto bufSize = blk->end() - blk->start();
                auto offset = blk->start();
                auto decoder = di::InstructionDecoder( bufStart, bufSize, Dyninst::Arch_x86_64 );
                
                while ( true ) {
                    auto currInst = decoder.decode();
                    if ( ! currInst.isValid() ) {
                        break;
                    }
                    if ( offset == targetAddress ) {
                        std::cout << "Located Instr: " << currInst.format() << std::endl;
                        auto fnNoConst = const_cast<dp::Function*>( f );
                        // Converting to Assignments, enabling StackAnalysis
                        Dyninst::AssignmentConverter ac( true, true );
                        std::vector<Dyninst::Assignment::Ptr> assignments;
                        ac.convert( currInst, offset, fnNoConst, blk, assignments );
                        for ( auto asgn: assignments ) {
                            std::cout << asgn->format() << std::endl;
                        }
                        break;
                    }
                    offset += currInst.size();
                }
                return 0;
            }
        }
    }
}

