// Filmton öffnen wie SDL_mixer (vorbisfile), einmal als Datenstrom (Weg aus native/src/music_hook.cpp)
// und optional spulbar (bisheriger Weg, hing auf der Quest). Läuft auf der Quest als ARM64-Programm
// gegen die echten Filmdateien:
//
//   test_movie_audio stream|seek|broken <datei.ogv> ...   (broken = Positionsfehler von Stratagus CFile)
//
// Ausgabe je Datei: Öffnungszeit, Tondauer, Dekodierzeit. „stream“ muss jede Datei schnell öffnen und
// komplett dekodieren; „seek“ dient nur dem Vergleich (mit timeout aufrufen).
#include <vorbis/vorbisfile.h>

#include <chrono>
#include <cstdio>
#include <cstring>

namespace {

size_t readFn(void* ptr, size_t size, size_t n, void* f) { return std::fread(ptr, size, n, static_cast<FILE*>(f)); }
int seekFn(void* f, ogg_int64_t off, int whence) { return std::fseek(static_cast<FILE*>(f), static_cast<long>(off), whence); }
long tellFn(void* f) { return std::ftell(static_cast<FILE*>(f)); }
// Wie Stratagus CFile::to_SDL_RWops: SDL_RWtell liefert dort den Rückgabewert von fseek (0), nicht die Position.
long brokenTellFn(void*) { return 0; }

double since(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "Aufruf: %s stream|seek <datei.ogv> ...\n", argv[0]);
    return 2;
  }
  const bool broken = std::strcmp(argv[1], "broken") == 0;
  const bool seekable = broken || std::strcmp(argv[1], "seek") == 0;
  int failures = 0;
  for (int i = 2; i < argc; ++i) {
    FILE* f = std::fopen(argv[i], "rb");
    if (!f) {
      std::printf("FEHLER: %s nicht lesbar\n", argv[i]);
      ++failures;
      continue;
    }
    ov_callbacks cb{readFn, seekable ? seekFn : nullptr, nullptr,
                    broken ? brokenTellFn : seekable ? tellFn : nullptr};
    OggVorbis_File vf;
    auto t0 = std::chrono::steady_clock::now();
    if (ov_open_callbacks(f, &vf, nullptr, 0, cb) < 0) {
      std::printf("FEHLER: %s kein Vorbis-Ton\n", argv[i]);
      std::fclose(f);
      ++failures;
      continue;
    }
    double openTime = since(t0);
    vorbis_info* vi = ov_info(&vf, -1);
    char buf[16384];
    long long bytes = 0;
    int section = 0;
    auto t1 = std::chrono::steady_clock::now();
    for (;;) {
      long n = ov_read(&vf, buf, sizeof(buf), 0, 2, 1, &section);
      if (n == 0) break;
      if (n < 0) continue;  // Lücke im Datenstrom: weiterlesen wie SDL_mixer
      bytes += n;
    }
    double seconds = vi && vi->rate && vi->channels ? static_cast<double>(bytes) / (2.0 * vi->channels * vi->rate) : 0;
    std::printf("%s: %s geöffnet in %.3f s, Ton %.1f s (%d Hz, %d Kanäle), dekodiert in %.2f s\n", argv[i],
                seekable ? "spulbar" : "Datenstrom", openTime, seconds, vi ? static_cast<int>(vi->rate) : 0,
                vi ? vi->channels : 0, since(t1));
    if (!seekable && (openTime > 1.0 || seconds < 0.5)) {
      std::printf("FEHLER: %s zu langsam geöffnet oder kein Ton\n", argv[i]);
      ++failures;
    }
    ov_clear(&vf);  // schließt f nicht (close_func = nullptr)
    std::fclose(f);
  }
  std::printf("test_movie_audio: %d Dateien, %d Fehler\n", argc - 2, failures);
  return failures ? 1 : 0;
}
