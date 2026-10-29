#pragma once

namespace waveshare_boot {

// Inputs use the compiler's fixed __DATE__ and __TIME__ formats.
inline void formatBuildDateTime(char (&out)[17], const char* date,
                                const char* time) {
  constexpr const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  unsigned month = 0;
  for (unsigned i = 0; i < 12; ++i) {
    if (date[0] == months[i][0] && date[1] == months[i][1] &&
        date[2] == months[i][2]) {
      month = i + 1;
      break;
    }
  }
  out[0] = date[7];
  out[1] = date[8];
  out[2] = date[9];
  out[3] = date[10];
  out[4] = '.';
  out[5] = static_cast<char>('0' + month / 10);
  out[6] = static_cast<char>('0' + month % 10);
  out[7] = '.';
  out[8] = date[4] == ' ' ? '0' : date[4];
  out[9] = date[5];
  out[10] = ' ';
  out[11] = time[0];
  out[12] = time[1];
  out[13] = ':';
  out[14] = time[3];
  out[15] = time[4];
  out[16] = '\0';
}

}  // namespace waveshare_boot
