#ifndef SYSTEMMIXER_FIXTURES_H
#define SYSTEMMIXER_FIXTURES_H

// ---------------------------------------------------------------------------
// Fixtures: real captures from Phase 0 / live verification (2026-10-12).
// Kept out of tst_systemmixer.cpp on purpose: moc's lexer does not treat raw
// string contents as raw and loses the Q_OBJECT class when a fixture contains
// an odd number of `"` characters. moc never processes this header.
// Raw strings use explicit delimiters because the JSON ends in `)]`.
// ---------------------------------------------------------------------------

static const char kPactlInfoJson[] = R"JSON({"server_string":"unix:/run/user/1000/pulse/native","library_protocol_version":35,"server_protocol_version":35,"is_local":"yes","client_index":185,"tile_size":"65472","user_name":"dmk","host_name":"dmk","server_name":"PulseAudio (on PipeWire 1.6.9)","server_version":"15.0.0","default_sample_specification":"float32le 2ch 48000Hz","default_channel_map":"front-left,front-right","default_sink_name":"alsa_output.pci-0000_00_1b.0.analog-stereo","default_source_name":"alsa_input.pci-0000_00_1b.0.analog-stereo","cookie":"b1c2:4905"})JSON";
static const char kPactlInfoText[] = R"TXT(Server String: unix:/run/user/1000/pulse/native
Library Protocol Version: 35
Server Protocol Version: 35
Is Local: yes
Client Index: 184
Tile Size: 65472
User Name: dmk
Host Name: dmk
Server Name: PulseAudio (on PipeWire 1.6.9)
Server Version: 15.0.0
Default Sample Specification: float32le 2ch 48000Hz
Default Channel Map: front-left,front-right
Default Sink: alsa_output.pci-0000_00_1b.0.analog-stereo
Default Source: alsa_input.pci-0000_00_1b.0.analog-stereo
Cookie: b1c2:4905
)TXT";
static const char kPactlSinksStereo[] = R"JSON([{"index":50,"state":"SUSPENDED","name":"alsa_output.pci-0000_00_1b.0.analog-stereo","description":"Built-in Audio Analog Stereo","driver":"PipeWire","sample_specification":"s32le 2ch 48000Hz","channel_map":"front-left,front-right","owner_module":4294967295,"mute":false,"volume":{"front-left":{"value":52429,"value_percent":"80%","db":"-5.81 dB"},"front-right":{"value":52429,"value_percent":"80%","db":"-5.81 dB"}},"balance":0.00,"base_volume":{"value":65536,"value_percent":"100%","db":"0.00 dB"},"monitor_source":"alsa_output.pci-0000_00_1b.0.analog-stereo.monitor","latency":{"actual":0.00,"configured":0.00},"flags":["HARDWARE","HW_MUTE_CTRL","HW_VOLUME_CTRL","DECIBEL_VOLUME","LATENCY"],"properties":{"device.icon_name":"audio-card-analog","device.bus":"pci","device.class":"sound","device.form_factor":"internal","device.profile.name":"analog-stereo","device.profile.description":"Analog Stereo","media.class":"Audio/Sink","node.name":"alsa_output.pci-0000_00_1b.0.analog-stereo","device.description":"Built-in Audio","object.id":"50","object.serial":"50"},"ports":[{"name":"analog-output-speaker","description":"Speakers","type":"Speaker","priority":10000,"availability_group":"Legacy 3","availability":"availability unknown"},{"name":"analog-output-headphones","description":"Headphones","type":"Headphones","priority":9900,"availability_group":"Legacy 4","availability":"not available"}],"active_port":"analog-output-speaker","formats":["pcm"]}])JSON";
static const char kPactlSinks51[] = R"JSON([{"index":1,"state":"RUNNING","name":"alsa_output.pci-0000_01_00.1.hdmi-stereo","description":"HDA NVidia HDMI","driver":"PipeWire","sample_specification":"s32le 8ch 48000Hz","channel_map":"front-left,front-right,rear-left,rear-right,front-center,lfe,side-left,side-right","owner_module":4294967295,"mute":false,"volume":{"front-left":{"value":65536,"value_percent":"100%","db":"0.00 dB"},"front-right":{"value":32768,"value_percent":"50%","db":"-18.00 dB"},"rear-left":{"value":49152,"value_percent":"75%","db":"-7.50 dB"},"rear-right":{"value":16384,"value_percent":"25%","db":"-24.00 dB"},"front-center":{"value":60000,"value_percent":"92%","db":"-1.00 dB"},"lfe":{"value":1000,"value_percent":"2%","db":"-52.00 dB"},"side-left":{"value":2000,"value_percent":"3%","db":"-46.00 dB"},"side-right":{"value":3000,"value_percent":"5%","db":"-42.00 dB"}},"balance":0.00,"base_volume":{"value":65536,"value_percent":"100%","db":"0.00 dB"},"flags":["HARDWARE","DECIBEL_VOLUME","LATENCY"],"properties":{"device.bus":"pci","media.class":"Audio/Sink","node.name":"alsa_output.pci-0000_01_00.1.hdmi-stereo"},"ports":[{"name":"hdmi-output-0","description":"HDMI / DisplayPort","type":"HDMI","priority":5900,"availability_group":"Legacy 1","availability":"available"}],"active_port":"hdmi-output-0","formats":["pcm"]}])JSON";
static const char kPactlSinkInputs[] = R"JSON([{"index":156,"driver":"PipeWire","owner_module":null,"client":"155","sink":50,"sample_specification":"float32le 2ch 44100Hz","channel_map":"front-left,front-right","format":"pcm, format.sample_format = \"float32le\"  format.rate = \"44100\"  format.channels = \"2\"  format.channel_map = \"front-left,front-right\"","corked":false,"mute":false,"volume":{"front-left":{"value":65536,"value_percent":"100%","db":"0.00 dB"},"front-right":{"value":65536,"value_percent":"100%","db":"0.00 dB"}},"balance":0.00,"properties":{"application.name":"pw-play","node.name":"pw-play","media.type":"Audio","media.category":"Playback","media.role":"music","media.filename":"/usr/share/sounds/freedesktop/stereo/complete.oga","stream.is-live":"true","media.class":"Stream/Output/Audio","client.id":74,"object.serial":156,"module-stream-restore.id":"sink-input-by-media-role:music"}}])JSON";
static const char kWpctlStatus[] = R"OUT(Audio
 ├─ Devices:
 │      46. Built-in Audio                      [alsa]
 │  
 ├─ Sinks:
 │  *   50. Built-in Audio Analog Stereo        [vol: 0.61]
 │  
 ├─ Sources:
 │  *   51. Built-in Audio Analog Stereo        [vol: 0.00 MUTED]
 │  
 ├─ Filters:
 │  
 └─ Streams:

Video
 ├─ Devices:
 │      52. Laptop_Integrated_Webcam_1.3M       [v4l2]
 │      53. Laptop_Integrated_Webcam_1.3M       [v4l2]
 │  
 ├─ Sinks:
 │  
 ├─ Sources:
 │  *   60. Laptop_Integrated_Webcam_1.3M (V4V2)
 │  
 ├─ Filters:
 │  
 └─ Streams:

Settings
 └─ Default Configured Devices:
)OUT";
static const char kWpctlInspect[] = R"OUT(id 50, type PipeWire:Interface:Node
    alsa.card = "0"
    alsa.card_name = "HDA Intel PCH"
    alsa.class = "generic"
    alsa.device = "0"
    alsa.driver_name = "snd_hda_intel"
    alsa.id = "PCH"
    api.alsa.pcm.card = "0"
    api.alsa.pcm.stream = "playback"
    audio.channels = "2"
    audio.position = "[ FL, FR ]"
    card.profile.device = "4"
  * client.id = "33"
    clock.quantum-limit = "8192"
    device.api = "alsa"
    device.bus = "pci"
    device.class = "sound"
    device.form-factor = "internal"
    device.profile.name = "analog-stereo"
  * media.class = "Audio/Sink"
  * node.description = "Built-in Audio Analog Stereo"
    node.driver = "true"
  * node.name = "alsa_output.pci-0000_00_1b.0.analog-stereo"
  * node.nick = "92HD87B1/3 Analog"
  * object.path = "alsa:acp:PCH:4:playback"
  * object.serial = "50"
  * priority.driver = "1009"
  * priority.session = "1009"
)OUT";

#endif // SYSTEMMIXER_FIXTURES_H
