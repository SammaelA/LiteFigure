#include "ttf_reader.h"
#include <cstdint>
#include <vector>
#include <fstream>
namespace LiteFigure
{
  template <typename T>
  T big_to_little_endian(const uint8_t *data)
  {
    uint8_t res[sizeof(T)] = {0};
    for (int i = 0; i < sizeof(T); i++)
    {
      res[i] = data[sizeof(T) - i - 1];
    }
    return *reinterpret_cast<T*>(res);
  }

  uint32_t CalcTableChecksum(uint32_t *table, uint32_t numberOfBytesInTable)
	{
    uint32_t sum = 0;
    uint32_t nLongs = (numberOfBytesInTable + 3) / 4;
    while (nLongs-- > 0)
      sum += *table++;
    return sum;
	}

  struct TTFOffsetSubtable
  {
    uint32_t scalerType = 0;
    uint16_t numTables = 0;
    uint16_t searchRange = 0;
    uint16_t entrySelector = 0;
    uint16_t rangeShift = 0;
  };

  struct TTFTableDirectory
  {
    uint32_t tag = 0;
    uint32_t checkSum = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
  };

  bool read_ttf_debug(const std::string &filename)
  {
    std::vector<uint8_t> buffer;
    TTFOffsetSubtable offsetSubtable;
    std::vector<TTFTableDirectory> table_directories;

    buffer.reserve(100000); //should be enough for most fonts
    std::ifstream file(filename, std::ios::binary);
    file.seekg(0, std::ios::end);
    buffer.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    file.close();

    printf("read %d bytes\n", (int)buffer.size());
    offsetSubtable.scalerType = big_to_little_endian<uint32_t>(buffer.data());
    offsetSubtable.numTables = big_to_little_endian<uint16_t>(buffer.data() + 4);
    offsetSubtable.searchRange = big_to_little_endian<uint16_t>(buffer.data() + 6);
    offsetSubtable.entrySelector = big_to_little_endian<uint16_t>(buffer.data() + 8);
    offsetSubtable.rangeShift = big_to_little_endian<uint16_t>(buffer.data() + 10);
    printf("scalerType   : 0x%08x\n", offsetSubtable.scalerType);
    printf("numTables    : %d\n", offsetSubtable.numTables);
    printf("searchRange  : %d\n", offsetSubtable.searchRange);
    printf("entrySelector: %d\n", offsetSubtable.entrySelector);
    printf("rangeShift   : %d\n", offsetSubtable.rangeShift);

    table_directories.resize(offsetSubtable.numTables);
    uint32_t cur_off = 12;
    for (int i = 0; i < offsetSubtable.numTables; i++)
    {
      TTFTableDirectory &table = table_directories[i];
      table.tag = big_to_little_endian<uint32_t>(buffer.data() + cur_off);
      table.checkSum = big_to_little_endian<uint32_t>(buffer.data() + cur_off + 4);
      table.offset = big_to_little_endian<uint32_t>(buffer.data() + cur_off + 8);
      table.length = big_to_little_endian<uint32_t>(buffer.data() + cur_off + 12);
      printf("table %02d: tag \'%c%c%c%c\', offset %5d, length %5d\n", i, 
             table.tag & 0xFF, (table.tag >> 8) & 0xFF, (table.tag >> 16) & 0xFF, (table.tag >> 24) & 0xFF, 
             table.offset, table.length);
      cur_off += 16;
    }
    return true;
  }
}