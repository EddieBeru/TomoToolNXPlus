#include "ugc_names.h"
#include "support/texture_browser_fixtures.h"
#include <cassert>
#include <iostream>
int main(){
    auto data=Fixtures::names();auto r=UgcNames::parse(data);
    assert(!r.warning && r.values.at("UgcFood000")=="A");
    auto bad=data;Fixtures::u32(bad,0x24,29);
    assert(UgcNames::parse(bad).warning);
    bad=data;Fixtures::u32(bad,0x2c,0xfffffffe);
    assert(UgcNames::parse(bad).warning);
    bad=data;Fixtures::u32(bad,0x40,100);
    assert(UgcNames::parse(bad).warning);
    bad=data;bad[0x44]=0x3d;bad[0x45]=0xd8;bad[0x46]=0;bad[0x47]=0xde;
    assert(UgcNames::parse(bad).values.at("UgcFood000")=="\xf0\x9f\x98\x80");
    bad=data;bad[0x45]=0xd8;assert(UgcNames::parse(bad).warning);
    bad=data;Fixtures::u32(bad,8,0x38);
    Fixtures::u32(bad,0x30,0x408494f5);Fixtures::u32(bad,0x34,0x40);
    auto duplicate=UgcNames::parse(bad);assert(duplicate.warning && duplicate.values.empty());
    assert(UgcNames::parse({}).warning);
    std::cout<<"ugc names: OK\n";
}
