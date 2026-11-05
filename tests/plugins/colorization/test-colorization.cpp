// license:GPLv3+

// VNI and Serum colorization at every DMD size standalone supports natively:
// 128x32, 128x16 and 192x64. The colorizations are synthetic one-frame ones
// built in this file, no shipped colorization data is used.

#include "core/stdafx.h"
#include "../../vpx-test.h"
#include "doctest.h"

#include "serum-decode.h"
#include "vni.h"

namespace
{

// zlib-style CRC32 (reflected, poly 0xEDB88320), the same variant libserum's
// crc32_fast uses for frame hashcodes.
uint32_t Crc32(const uint8_t* data, size_t size)
{
   uint32_t crc = 0xffffffffu;
   for (size_t i = 0; i < size; i++)
   {
      crc ^= data[i];
      for (int k = 0; k < 8; k++)
         crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
   }
   return ~crc;
}

// --- Minimal Serum colorizations -----------------------------------------
//
// libserum builds a colorization from the blocks an authoring tool hands over
// through Serum_LoadFromSource(). FlatSource serves one frame, colorized to a
// single flat color, identified by the CRC32 of the ROM frame (no mask, no
// shape mode), with no extra plane, sprites, backgrounds, rotations or
// settings.

constexpr uint16_t kSerumColor = 0xf81f; // magenta, RGB565

// Serum v2: RGB565 planes, base height must be 32 or 64. FRAME_COLORS holds
// `color` pixels everywhere.
struct FlatSource
{
   Serum_Colorization_Source source {};
   uint32_t hash = 0;
   uint16_t color = kSerumColor;

   FlatSource(uint32_t w, uint32_t h, const std::vector<uint8_t>& romFrame, uint16_t c = kSerumColor)
      : hash(Crc32(romFrame.data(), romFrame.size()))
      , color(c)
   {
      strncpy(source.romName, "flat", sizeof(source.romName) - 1);
      source.width = w;
      source.height = h;
      source.frames = 1;
      source.colors = 4;
      source.spritesPerFrame = LEGACY_SPRITES_PER_FRAME;
      source.fill = &Fill;
      source.userData = this;
   }

   static void Fill16(void* out, size_t size, uint16_t v)
   {
      uint16_t* p = static_cast<uint16_t*>(out);
      for (size_t i = 0; i < size / 2; i++)
         p[i] = v;
   }

   static void SERUM_CALLBACK Fill(void* userData, Serum_Block block, uint32_t, void* out, size_t size)
   {
      const FlatSource& self = *static_cast<const FlatSource*>(userData);
      memset(out, 0, size);
      switch (block)
      {
      case SERUM_BLOCK_FRAME_HASH: memcpy(out, &self.hash, sizeof(self.hash)); break;
      case SERUM_BLOCK_FRAME_MASK_ID: // no comparison mask
      case SERUM_BLOCK_FRAME_DYNAMIC_MASK: // no dynamic zones
      case SERUM_BLOCK_FRAME_DYNAMIC_MASK_EXTRA: memset(out, 255, size); break;
      case SERUM_BLOCK_FRAME_COLORS: Fill16(out, size, self.color); break;
      case SERUM_BLOCK_FRAME_SPRITES: Fill16(out, size, SERUM_NO_SPRITE); break;
      case SERUM_BLOCK_FRAME_ACTIVE: memset(out, 1, size); break;
      case SERUM_BLOCK_FRAME_TRIGGER: memset(out, 255, size); break; // no trigger
      case SERUM_BLOCK_FRAME_SPRITE_BOUNDS: Fill16(out, size, 0xffff); break;
      case SERUM_BLOCK_FRAME_BACKGROUND_ID: Fill16(out, size, 0xffff); break; // no background
      default: break;
      }
   }
};

Serum_Frame_Struc* LoadFlat(uint32_t w, uint32_t h, const std::vector<uint8_t>& romFrame)
{
   FlatSource source(w, h, romFrame);
   return Serum_LoadFromSource(&source.source, nullptr, FLAG_REQUEST_32P_FRAMES | FLAG_REQUEST_64P_FRAMES);
}

std::vector<uint8_t> Centered128x32(const std::vector<uint8_t>& frame128x16)
{
   std::vector<uint8_t> out(128 * 32, 0);
   memcpy(out.data() + 128 * 8, frame128x16.data(), 128 * 16);
   return out;
}

// --- Minimal pin2dmd .pal writer ------------------------------------------
//
// One palette, no mappings, no masks: libvni colorizes every frame with it.

void P8(std::vector<uint8_t>& out, uint8_t v) { out.push_back(v); }
void PB16(std::vector<uint8_t>& out, uint16_t v)
{
   out.push_back(static_cast<uint8_t>(v >> 8));
   out.push_back(static_cast<uint8_t>(v));
}

std::vector<uint8_t> MakePal()
{
   std::vector<uint8_t> out;
   P8(out, 4); // version
   PB16(out, 1); // one palette
   PB16(out, 0); // palette index 0
   PB16(out, 4); // four colors
   P8(out, 1); // type 1: default palette
   P8(out, 0);
   P8(out, 0);
   P8(out, 0);
   P8(out, 10);
   P8(out, 20);
   P8(out, 30);
   P8(out, 40);
   P8(out, 50);
   P8(out, 60);
   P8(out, 70);
   P8(out, 80);
   P8(out, 90);
   PB16(out, 0); // no mappings
   P8(out, 0); // no masks
   return out;
}

std::string g_serumLog;

void SerumLog(const char* format, va_list args, const void*)
{
   char buf[1024];
   vsnprintf(buf, sizeof(buf), format, args);
   g_serumLog += buf;
   g_serumLog += '\n';
}

} // namespace

void SerumTestSetup()
{
   static bool once = false;
   if (!once)
   {
      once = true;
      Serum_SetLogCallback(SerumLog, nullptr);
   }
   g_serumLog.clear();
}

TEST_CASE("Serum colorization: 128x32")
{
   SerumTestSetup();
   std::vector<uint8_t> rom(128 * 32, 3);
   Serum_Frame_Struc* serum = LoadFlat(128, 32, rom);
   INFO("load error: ", std::string(Serum_GetLastErrorMessage()));
   INFO("serum log:\n", g_serumLog);
   REQUIRE(serum != nullptr);
   CHECK(serum->SerumVersion == SERUM_V2);

   std::vector<uint8_t> frame = rom;
   CHECK(Serum_Colorize(frame.data()) != IDENTIFY_NO_FRAME);
   CHECK((serum->flags & FLAG_RETURNED_32P_FRAME_OK) != 0);
   REQUIRE(serum->frame32 != nullptr);
   CHECK(serum->width32 == 128);
   CHECK(serum->frame32[0] == kSerumColor);
   CHECK(serum->frame32[128 * 32 - 1] == kSerumColor);
   Serum_Dispose();
}

TEST_CASE("Serum colorization: 128x16")
{
   SerumTestSetup();
   // A v2 colorization for a 16-row ROM is authored on 32-row frames: the 16
   // rows are stored centered between 8 blank rows top and bottom, and the
   // stored hash is over that 128x32 frame.
   std::vector<uint8_t> rom16(128 * 16, 3);
   std::vector<uint8_t> rom32 = Centered128x32(rom16);
   Serum_Frame_Struc* serum = LoadFlat(128, 32, rom32);
   INFO("load error: ", std::string(Serum_GetLastErrorMessage()));
   INFO("serum log:\n", g_serumLog);
   REQUIRE(serum != nullptr);
   CHECK(serum->SerumVersion == SERUM_V2);

   // The raw 128x16 frame cannot be identified; the frame the colorization was
   // authored on can be. libserum always reads a full 128x32 frame, so the raw
   // rows are passed at the top of a zeroed 128x32 buffer, not centered.
   std::vector<uint8_t> frame = rom16;
   frame.resize(128 * 32, 0);
   CHECK(Serum_Colorize(frame.data()) == IDENTIFY_NO_FRAME);
   frame = Centered128x32(rom16);
   CHECK(Serum_Colorize(frame.data()) != IDENTIFY_NO_FRAME);
   CHECK((serum->flags & FLAG_RETURNED_32P_FRAME_OK) != 0);
   REQUIRE(serum->frame32 != nullptr);
   CHECK(serum->frame32[0] == kSerumColor);
   Serum_Dispose();
}

TEST_CASE("Serum colorization: 192x64")
{
   SerumTestSetup();
   std::vector<uint8_t> rom(192 * 64, 3);
   Serum_Frame_Struc* serum = LoadFlat(192, 64, rom);
   INFO("load error: ", std::string(Serum_GetLastErrorMessage()));
   REQUIRE(serum != nullptr);
   CHECK(serum->SerumVersion == SERUM_V2);

   std::vector<uint8_t> frame = rom;
   CHECK(Serum_Colorize(frame.data()) != IDENTIFY_NO_FRAME);
   CHECK((serum->flags & FLAG_RETURNED_64P_FRAME_OK) != 0);
   REQUIRE(serum->frame64 != nullptr);
   CHECK(serum->width64 == 192);
   CHECK(serum->frame64[0] == kSerumColor);
   Serum_Dispose();
}

// The host loads a colorization through Serum_Load(), which takes
// <altcolor>/<rom>/<rom>.cROMc, the file the author exports, and nothing else.
TEST_CASE("Serum colorization: cROMc file")
{
   SerumTestSetup();
   const std::filesystem::path altcolor = std::filesystem::temp_directory_path() / "vpx_test_serum_altcolor";
   std::filesystem::remove_all(altcolor);
   std::filesystem::create_directories(altcolor / "flat");
   std::vector<uint8_t> rom(128 * 32, 3);
   FlatSource source(128, 32, rom);

   // A raw cROM or cRZ beside it is not a colorization.
   {
      std::ofstream(altcolor / "flat" / "flat.cRZ", std::ios::binary) << "raw";
   }
   CHECK(Serum_Load(altcolor.string().c_str(), "flat", FLAG_REQUEST_32P_FRAMES | FLAG_REQUEST_64P_FRAMES) == nullptr);

   REQUIRE_MESSAGE(Serum_ConcentrateFromSource(&source.source, (altcolor / "flat" / "flat.cROMc").string().c_str()), Serum_GetLastErrorMessage());
   Serum_Frame_Struc* serum = Serum_Load(altcolor.string().c_str(), "flat", FLAG_REQUEST_32P_FRAMES | FLAG_REQUEST_64P_FRAMES);
   INFO("serum log:\n", g_serumLog);
   REQUIRE(serum != nullptr);
   CHECK(serum->SerumVersion == SERUM_V2);

   std::vector<uint8_t> frame = rom;
   CHECK(Serum_Colorize(frame.data()) != IDENTIFY_NO_FRAME);
   REQUIRE(serum->frame32 != nullptr);
   CHECK(serum->frame32[0] == kSerumColor);
   Serum_Dispose();
   std::filesystem::remove_all(altcolor);
}

TEST_CASE("VNI colorization")
{
   const std::vector<uint8_t> pal = MakePal();
   const std::filesystem::path palPath = std::filesystem::temp_directory_path() / "vpx_test_pin2dmd.pal";
   {
      std::ofstream f(palPath, std::ios::binary | std::ios::trunc);
      f.write(reinterpret_cast<const char*>(pal.data()), static_cast<std::streamsize>(pal.size()));
   }
   Vni_Context* vni = Vni_LoadFromPaths(palPath.string().c_str(), nullptr, nullptr, nullptr);
   REQUIRE(vni != nullptr);

   struct Case
   {
      uint32_t w, h, outW, outH;
   };
   const Case cases[] = {
      { 128, 32, 128, 32 }, // native
      { 128, 16, 128, 32 }, // centered into a 128x32 frame by libvni
      { 192, 64, 192, 64 }, // native
   };
   for (const Case& c : cases)
   {
      std::vector<uint8_t> frame(static_cast<size_t>(c.w) * c.h, 3);
      CHECK(Vni_Colorize(vni, frame.data(), c.w, c.h, 2) != 0);
      const Vni_Frame_Struc* out = Vni_GetFrame(vni);
      REQUIRE(out != nullptr);
      REQUIRE(out->has_frame != 0);
      CHECK(out->width == c.outW);
      CHECK(out->height == c.outH);
      // first content pixel: for a 128x16 input centered in 128x32 the top 8
      // rows are padding
      CHECK(out->frame[static_cast<size_t>((c.outH - c.h) / 2) * c.outW] == 3);
      CHECK(out->frame[static_cast<size_t>(c.outW) * c.outH - 1] == (c.outH == c.h ? 3 : 0));
      // Palette index 3 is RGB(70,80,90)
      CHECK(out->palette[9] == 70);
      CHECK(out->palette[10] == 80);
      CHECK(out->palette[11] == 90);
   }

   Vni_Dispose(vni);
   std::filesystem::remove(palPath);
}
