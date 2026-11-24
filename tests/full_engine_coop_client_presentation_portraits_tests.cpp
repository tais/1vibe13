#include "Ja2/FullEngineCoopClientPresentationPortraits.h"
#include "Ja2/FullEngineCoopClientPresentationPassiveUi.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (false)
using Portrait = TacticalPortraitSnapshot;
using Family = TacticalPortraitFamily;
using Camo = TacticalPortraitCamouflage;
using Visible = FullEngineCoopClientVisiblePortrait;
using Cache = FullEngineCoopClientPresentationPortraits;
using Rect = FullEngineCoopClientPassiveUiRect;

void TestPaths()
{
	const std::array<const char*,5> directories{{"", "WoodCamo\\", "UrbanCamo\\", "DesertCamo\\", "SnowCamo\\"}};
	for (const auto family : {Family::Faces, Family::ImpFaces})
		for (std::uint8_t camo = 0; camo < directories.size(); ++camo)
			for (std::uint8_t face : {std::uint8_t(0),std::uint8_t(9),std::uint8_t(99),std::uint8_t(100),std::uint8_t(151),std::uint8_t(255)})
			{
				const Portrait portrait{family, face, static_cast<Camo>(camo)};
				for (const bool base : {false,true})
				{
					std::array<char,40> path{};
					char expected[40];
					std::snprintf(expected,sizeof(expected),"%s\\%s%02u.sti",
						family == Family::Faces ? "FACES" : "IMPFACES", base ? "" : directories[camo],unsigned(face));
					CHECK(BuildFullEngineCoopClientPortraitAssetPath(portrait,base,path.data(),path.size()) &&
						std::strcmp(path.data(),expected) == 0,
						"authority-only family, face index and camo produce exact two/three-digit stock path");
					path.fill('x');
					const auto saved = path;
					CHECK(!BuildFullEngineCoopClientPortraitAssetPath(portrait,base,path.data(),std::strlen(expected)) && path == saved,
						"short path output preserves its contents including terminator space");
				}
			}
	for (const Portrait invalid : {Portrait{}, Portrait{Family::Absent,1,Camo::None},
		Portrait{Family::Absent,0,Camo::Wood}, Portrait{static_cast<Family>(3),1,Camo::None},
		Portrait{Family::Faces,1,static_cast<Camo>(5)}})
	{
		std::array<char,40> path; path.fill('x'); const auto saved=path;
		CHECK(!BuildFullEngineCoopClientPortraitAssetPath(invalid,false,path.data(),path.size()) && path == saved,
			"absent and malformed descriptors never invent a fallback identity");
	}
	CHECK(!BuildFullEngineCoopClientPortraitAssetPath({Family::Faces,1,Camo::None},false,nullptr,40),
		"null path output fails");
}

void TestFrames()
{
	const std::uint8_t valid[]{2,1,2,0,0x82,0};
	CHECK(IsValidFullEngineCoopClientPortraitFrame(valid,sizeof(valid),2,2),
		"literal and transparent rows form a bounded complete ETRLE frame");
	for (const std::vector<std::uint8_t> invalid : {
		std::vector<std::uint8_t>{2,1}, {2,1,2}, {2,1,2,0}, {3,1,2,3,0,0x82,0},
		{0x80,0,0x82,0}, {1,1,0,0x82,0}, {2,1,2,0,0x82,0,0}, {0x82,0,0x82}})
		CHECK(!IsValidFullEngineCoopClientPortraitFrame(invalid.data(),invalid.size(),2,2),
			"truncation, zero run, row overrun/underrun, missing terminator and trailing bytes fail before blitting");
	std::vector<std::uint8_t> maximum;
	for (unsigned row=0;row<43;++row)
	{ maximum.push_back(48); for(unsigned x=0;x<48;++x)maximum.push_back(std::uint8_t(x)); maximum.push_back(0); }
	CHECK(IsValidFullEngineCoopClientPortraitFrame(maximum.data(),maximum.size(),48,43),
		"complete stock-sized frame is accepted without reading outside its data length");
	CHECK(!IsValidFullEngineCoopClientPortraitFrame(nullptr,6,2,2) &&
		!IsValidFullEngineCoopClientPortraitFrame(valid,0,2,2) &&
		!IsValidFullEngineCoopClientPortraitFrame(valid,6,0,2) &&
		!IsValidFullEngineCoopClientPortraitFrame(valid,6,2,0) &&
		!IsValidFullEngineCoopClientPortraitFrame(valid,6,49,2) &&
		!IsValidFullEngineCoopClientPortraitFrame(valid,6,2,44),
		"bad pointer, length and dimensions stay neutral");
}

struct Fake
{
	std::uint32_t next=1;
	std::map<std::uint32_t,std::string> live;
	std::vector<std::string> loads;
	std::vector<std::uint32_t> releases;
	std::vector<Rect> clips;
	std::set<std::string> missing;
	bool blitSucceeds=true;
	static Fake& get(void* context) { return *static_cast<Fake*>(context); }
	static bool load(const char* path,std::uint32_t& output,void* context) noexcept
	{
		auto& self=get(context); self.loads.emplace_back(path);
		if(self.missing.count(path))return false;
		output=self.next++; self.live.emplace(output,path);
		CHECK(self.live.size()<=6,"native portrait handles never exceed six visible slots"); return true;
	}
	static void release(std::uint32_t handle,void* context) noexcept
	{
		auto& self=get(context); CHECK(self.live.erase(handle)==1,"release only an owned live portrait handle");
		self.releases.push_back(handle);
	}
	static bool blit(std::uint32_t handle,const Rect& rect,void* context) noexcept
	{
		auto& self=get(context); CHECK(self.live.count(handle)==1,"blit only an owned portrait handle");
		self.clips.push_back(rect); return self.blitSucceeds;
	}
	FullEngineCoopClientPortraitServices services() { return {load,release,blit,this}; }
};

std::array<Visible,6> Page(std::uint16_t first=0)
{
	std::array<Visible,6> page{};
	for(std::uint16_t i=0;i<6;++i)page[i]={{std::uint16_t(first+i),1},{Family::Faces,std::uint8_t(first+i),Camo::None}};
	return page;
}

void TestOwnershipAndRevocation()
{
	Fake fake;
	{
		Cache cache(fake.services()); auto page=Page();
		CHECK(cache.synchronize(1,page.data(),page.size()) && fake.loads.size()==6,"initial visible page acquires six exact portraits");
		for(unsigned i=0;i<10;++i)CHECK(cache.synchronize(1,page.data(),page.size()),"same page remains valid");
		CHECK(fake.loads.size()==6 && fake.releases.empty(),"same identities do not reload every tactical revision");
		const Rect face{20,30,68,73};
		CHECK(cache.render(0,face) && fake.clips.back().left==20 && fake.clips.back().right==68,
			"renderer receives only the exact native 48x43 face rectangle");
		CHECK(!cache.render(6,face) && !cache.render(0,{20,30,69,73}) && !cache.render(0,{-1,30,47,73}),
			"bad slot/rectangle cannot reach the native blitter");
		page[0].actor.incarnation=2;
		CHECK(cache.synchronize(1,page.data(),6) && fake.releases.size()==1 && fake.loads.size()==7,
			"recycled actor identity revokes its previous portrait immediately");
		page[0].portrait.faceIndex=30;
		CHECK(cache.synchronize(1,page.data(),6) && fake.releases.size()==2 && fake.loads.size()==8,
			"descriptor replacement releases old artwork without waiting for drawing");
		CHECK(cache.synchronize(2,page.data(),6) && fake.releases.size()==8 && fake.loads.size()==14,
			"new world generation cannot retain old native handles");
		page=Page(6);
		CHECK(cache.synchronize(2,page.data(),6) && fake.releases.size()==14 && fake.loads.size()==20,
			"page change replaces all six cached identities before the next frame");
		CHECK(cache.synchronize(2,page.data(),2) && fake.live.size()==2 && fake.releases.size()==18,
			"assignment shrink releases invisible faces immediately");
		CHECK(!cache.synchronize(2,page.data(),7) && fake.live.empty(),"oversized roster revokes all cached portraits");
		CHECK(cache.synchronize(2,page.data(),2),"valid page can recover after cache revocation");
		page[1].actor=page[0].actor;
		CHECK(!cache.synchronize(2,page.data(),2) && fake.live.empty(),"duplicate actor page cannot retain mixed identities");
		page=Page(); CHECK(cache.synchronize(2,page.data(),2),"restore valid cache");
		page[1].portrait.camouflage=static_cast<Camo>(255);
		CHECK(!cache.synchronize(2,page.data(),2) && fake.live.empty(),"malformed descriptor clears the whole cache");
		page=Page(); CHECK(cache.synchronize(2,page.data(),2),"restore valid cache before destructor");
	}
	CHECK(fake.live.empty(),"destructor releases all remaining portrait handles");
}

void TestFailuresAndSameActorFallback()
{
	Fake fake;
	const Visible visible{{1,2},{Family::ImpFaces,151,Camo::Wood}};
	fake.missing.insert("IMPFACES\\WoodCamo\\151.sti");
	Cache cache(fake.services());
	CHECK((cache.synchronize(1,&visible,1) && fake.loads==std::vector<std::string>{"IMPFACES\\WoodCamo\\151.sti","IMPFACES\\151.sti"}),
		"missing camo falls back only to the same authority portrait's base variant");
	CHECK(cache.synchronize(1,&visible,1) && fake.loads.size()==2 && cache.render(0,{0,0,48,43}),
		"successful same-actor fallback is cached");
	fake.blitSucceeds=false;
	CHECK(!cache.render(0,{0,0,48,43}) && fake.live.empty(),"failed native validation/blit releases unusable artwork");
	const auto blits=fake.clips.size();
	CHECK(cache.synchronize(1,&visible,1) && !cache.render(0,{0,0,48,43}) &&
		fake.loads.size()==2 && fake.clips.size()==blits,"failed resource stays neutral without repeated loads or draws");
	fake.missing.insert("IMPFACES\\151.sti");
	CHECK(cache.synchronize(2,&visible,1) && fake.loads.size()==4 && fake.live.empty(),
		"new world retries both variants once but never substitutes another merc");
	for(unsigned i=0;i<5;++i)CHECK(cache.synchronize(2,&visible,1),"missing optional artwork does not fail synchronization");
	CHECK(fake.loads.size()==4,"two missing variants are negatively cached");
	const Visible absent{{1,2},{}};
	CHECK(cache.synchronize(2,&absent,1) && fake.loads.size()==4 && !cache.render(0,{0,0,48,43}),
		"explicit absent descriptor keeps neutral silhouette without any filename lookup");
	CHECK(!cache.synchronize(0,&visible,1) && !cache.synchronize(2,nullptr,1),"missing generation/list fails closed");
	Cache noServices({});
	CHECK(noServices.synchronize(1,&visible,1) && !noServices.render(0,{0,0,48,43}),
		"optional unavailable portrait services preserve the tactical UI");
}
}

int main()
{
	TestPaths(); TestFrames(); TestOwnershipAndRevocation(); TestFailuresAndSameActorFallback();
	std::printf("passive portrait cache/path/frame validation: %s (%d failures)\n",failures ? "FAIL" : "PASS",failures);
	return failures ? 1 : 0;
}
