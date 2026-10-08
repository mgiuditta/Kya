project(Audio)

add_library(Audio
	AudioPack.cpp
	AudioPack.h
	edMusicData.cpp
	edMusicData.h
	edMusicSynth.cpp
	edMusicSynth.h
	edMusicService.cpp
	edMusicService.h
	edSoundSampleService.cpp
	edSoundSampleService.h
	edSoundDevice.h
	edSoundStreamService.cpp
	edSoundStreamService.h
	edSysTransferService.cpp
	edSysTransferService.h)

target_include_directories(Audio PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

target_link_libraries(Audio PUBLIC Log)
# Audio packs hash PCM with the xxHash copy vendored by TextureUpload.
target_include_directories(Audio PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../Windows/Renderer/Vulkan/src/pcsx2/TextureUpload/ext/xxHash")

if(WIN32)
	target_link_libraries(Audio PRIVATE XAudio2 ole32)
elseif(APPLE)
	target_sources(Audio PRIVATE edSoundDeviceMiniaudio.cpp)
	target_include_directories(Audio PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../ext/miniaudio")
	target_link_libraries(Audio PRIVATE "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
endif()

add_executable(KyaMusicRender music_render.cpp)
target_link_libraries(KyaMusicRender PRIVATE Audio)
