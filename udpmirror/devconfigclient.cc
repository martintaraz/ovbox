#include "RSJparser.tcc"
#include "common.h"
#include "udpsocket.h"
#include <alsa/asoundlib.h>
#include <curl/curl.h>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

struct snddevname_t {
  std::string dev;
  std::string desc;
};

std::vector<snddevname_t> listdev()
{
  std::vector<snddevname_t> retv;
  char** hints;
  int err;
  char** n;
  char* name;
  char* desc;

  /* Enumerate sound devices */
  err = snd_device_name_hint(-1, "pcm", (void***)&hints);
  if(err != 0) {
    return retv;
  }
  n = hints;
  while(*n != NULL) {
    name = snd_device_name_get_hint(*n, "NAME");
    desc = snd_device_name_get_hint(*n, "DESC");
    if(strncmp("hw:", name, 3) == 0) {
      snddevname_t dname;
      dname.dev = name;
      dname.desc = desc;
      if(dname.desc.find("\n"))
        dname.desc.erase(dname.desc.find("\n"));
      retv.push_back(dname);
    }
    if(name && strcmp("null", name))
      free(name);
    if(desc && strcmp("null", desc))
      free(desc);
    n++;
  }
  // Free hint buffer too
  snd_device_name_free_hint((void**)hints);
  return retv;
}

CURL* curl;
static bool quit_app(false);

struct jacksettings_t {
  std::string device;
  int rate;
  int period;
  int buffers;
};

namespace webCURL {

  struct MemoryStruct {
    char* memory;
    size_t size;
  };

  static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb,
                                    void* userp)
  {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;
    char* ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
      /* out of memory! */
      printf("not enough memory (realloc returned NULL)\n");
      return 0;
    }
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
  }

} // namespace webCURL

std::string get_device_info(std::string url, const std::string& device,
                            std::string& hash)
{
  char chost[1024];
  memset(chost, 0, 1024);
  std::string hostname;
  if(0 == gethostname(chost, 1023))
    hostname = chost;
  CURLcode res;
  std::string retv;
  struct webCURL::MemoryStruct chunk;
  chunk.memory =
      (char*)malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;       /* no data at this point */

  url += "?dev=" + device + "&hash=" + hash;
  if(hostname.size() > 0)
    url += "&host=" + hostname;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERPWD, "device:device");
  /* send all data to this function  */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, webCURL::WriteMemoryCallback);
  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

  /* get it! */
  res = curl_easy_perform(curl);

  /* check for errors */
  if(res == CURLE_OK) {
    retv.insert(0, chunk.memory, chunk.size);
    // printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
  }
  free(chunk.memory);

  std::stringstream ss(retv);
  std::string to;
  bool first(true);
  retv = "";
  while(std::getline(ss, to, '\n')) {
    if(first)
      hash = to;
    else
      retv += to + '\n';
    first = false;
  }
  return retv;
}

void device_init_ver(std::string url, const std::string& device)
{
  struct webCURL::MemoryStruct chunk;
  chunk.memory =
      (char*)malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;       /* no data at this point */
  url += "?setver=" + device + "&ver=ovbox-" + OVBOXVERSION;
  curl_easy_reset(curl);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERPWD, "device:device");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, webCURL::WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  curl_easy_perform(curl);
  free(chunk.memory);
}

jacksettings_t get_device_init(std::string url, const std::string& device)
{
  std::vector<snddevname_t> alsadevs(listdev());
  std::string jsdevs("{");
  for(auto d : alsadevs)
    jsdevs += "\"" + d.dev + "\":\"" + d.desc + "\",";
  if(jsdevs.size())
    jsdevs.erase(jsdevs.end() - 1);
  jsdevs += "}";
  std::cout << jsdevs << std::endl;
  CURLcode res;
  std::string retv;
  struct webCURL::MemoryStruct chunk;
  chunk.memory =
      (char*)malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;       /* no data at this point */

  url += "?devinit=" + device;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERPWD, "device:device");
  /* send all data to this function  */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, webCURL::WriteMemoryCallback);
  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsdevs.c_str());

  /* get it! */
  res = curl_easy_perform(curl);

  /* check for errors */
  if(res == CURLE_OK) {
    retv.insert(0, chunk.memory, chunk.size);
    // printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
  }
  free(chunk.memory);
  // parse retv
  RSJresource my_json(retv);
  jacksettings_t jacks;
  jacks.device = my_json["jackdevice"].as<std::string>("hw:1");
  jacks.rate = my_json["jackrate"].as<int>(48000);
  jacks.period = my_json["jackperiod"].as<int>(96);
  jacks.buffers = my_json["jackbuffers"].as<int>(2);
  return jacks;
}

static void sighandler(int sig)
{
  quit_app = true;
}

int main(int argc, char** argv)
{
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);
  signal(SIGINT, &sighandler);
  try {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(!curl)
      throw ErrMsg("Unable to initialize curl");
    std::string lobby("https://oldbox.orlandoviols.com/");
    std::string device(getmacaddr());
    double gracetime(10);
    const char* options = "d:hl:g:";
    struct option long_options[] = {{"device", 1, 0, 'd'},
                                    {"lobbyurl", 1, 0, 'l'},
                                    {"help", 0, 0, 'h'},
                                    {"gracetime", 1, 0, 'g'},
                                    {0, 0, 0, 0}};
    int opt(0);
    int option_index(0);
    while((opt = getopt_long(argc, argv, options, long_options,
                             &option_index)) != -1) {
      switch(opt) {
      case 'h':
        app_usage("devconfigclient", long_options, "");
        return 0;
      case 'd':
        device = optarg;
        break;
      case 'l':
        lobby = optarg;
        break;
      case 'g':
        gracetime = atof(optarg);
        break;
      }
    }
    std::cerr << "Connecting to " << lobby << " for device " << device << "."
              << std::endl;
    std::string hash;
    FILE* h_pipe(NULL);
    FILE* h_pipe_jack(NULL);
    device_init_ver(lobby, device);
    jacksettings_t jacks(get_device_init(lobby, device));
    if((jacks.device.size() > 0) && (jacks.device != "manual")) {
      char cmd[1024];
      sprintf(cmd,
              "JACK_NO_AUDIO_RESERVATION=1 jackd --sync -P 40 -d alsa -d %s "
              "-r %d -p %d -n %d",
              jacks.device.c_str(), jacks.rate, jacks.period, jacks.buffers);
      std::cout << cmd << std::endl;
      h_pipe_jack = popen(cmd, "w");
      sleep(4);
    }
    while(!quit_app) {
      std::string tsc = get_device_info(lobby, device, hash);
      if((tsc.size() > 10) && (tsc.substr(0, 5) == "<?xml")) {
        // close current TASCAR session:
        if(h_pipe)
          fclose(h_pipe);
        h_pipe = NULL;
        // write new session file:
        std::ofstream ofh("session.tsc");
        ofh << tsc;
        ofh.close();
        // reopen TASCAR:
        h_pipe = popen("tascar_cli session.tsc", "w");
      }
      sleep(gracetime);
    }
    // close TASCAR::
    if(h_pipe)
      fclose(h_pipe);
    h_pipe = NULL;
    if(h_pipe_jack) {
      h_pipe = popen("killall jackd", "w");
      fclose(h_pipe_jack);
      fclose(h_pipe);
    }
    h_pipe_jack = NULL;
    curl_easy_cleanup(curl);
    curl_global_cleanup();
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
