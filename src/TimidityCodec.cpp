/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <kodi/audiodecoder/AudioDecoder.h>
#include <kodi/General.h>
#include <kodi/VFS.h>

#include <unistd.h>

extern "C" {
#include "timidity_codec.h"
}

class CTimidityCodecAddon
  : public kodi::addon::CInstanceAudioDecoder
{
public:
  CTimidityCodecAddon(KODI_HANDLE instance);
  virtual ~CTimidityCodecAddon();

  virtual bool Init(std::string file, unsigned int filecache, AUDIODEC_STREAM_INFO& streamInfo);
  virtual int64_t Seek(int64_t time);
  virtual int ReadPCM(uint8_t* buffer, int size, int& actualsize);

private:
  MidiSong* m_song;
  size_t m_pos;
};

CTimidityCodecAddon::CTimidityCodecAddon(KODI_HANDLE instance)
  : CInstanceAudioDecoder(instance),
    m_song(nullptr)
{
}

CTimidityCodecAddon::~CTimidityCodecAddon()
{
  if (m_song)
    Timidity_FreeSong(m_song);
}

bool CTimidityCodecAddon::Init(std::string filename, unsigned int filecache, AUDIODEC_STREAM_INFO& streamInfo)
{
  std::string soundfont;
  kodi::GetSettingString("soundfont", soundfont);
  if (soundfont.empty())
    kodi::OpenSettings();

  int res;
  if (soundfont == "sf2")
    res = Timidity_Init(48000, 16, 2, soundfont.c_str(), NULL); // real soundfont
  else
    res = Timidity_Init(48000, 16, 2, NULL, soundfont.c_str()); // config file

  if (res != 0)
    return false;

  kodi::vfs::CFile file;
  if (!file.OpenFile(filename))
    return false;

  int len = file.GetLength();
  uint8_t* data = new uint8_t[len];
  if (!data)
    return false;

  file.Read(data, len);

  const char* tempfile = tmpnam(NULL);

  FILE* f = fopen(tempfile,"wb");
  if (!f)
  {
    delete[] data;
    return false;
  }
  fwrite(data, 1, len, f);
  fclose(f);
  delete[] data;

  m_song = Timidity_LoadSong((char*)tempfile);
  unlink(tempfile);
  if (!m_song)
    return false;

  m_pos = 0;

  streamInfo.channels = 2;
  streamInfo.samplerate = 48000;
  streamInfo.bitspersample = 16;
  streamInfo.totaltime  = Timidity_GetLength(m_song);
  streamInfo.format = AUDIO_FMT_S16NE;
  streamInfo.channellist = { AUDIO_CH_FL, AUDIO_CH_FR, AUDIO_CH_NULL };
  streamInfo.bitrate = 0;
  return true;
}

int64_t CTimidityCodecAddon::Seek(int64_t time)
{
  return Timidity_Seek(m_song, time);
}

int CTimidityCodecAddon::ReadPCM(uint8_t* buffer, int size, int& actualsize)
{
  if (m_pos > Timidity_GetLength(m_song)/1000*48000*4)
    return -1;

  actualsize = Timidity_FillBuffer(m_song, buffer, size);
  m_pos += actualsize;

  return 0;
}

class CMyAddon : public ::kodi::addon::CAddonBase
{
public:
  CMyAddon() { }
  virtual ADDON_STATUS CreateInstance(int instanceType,
                                      std::string instanceID,
                                      KODI_HANDLE instance,
                                      KODI_HANDLE& addonInstance) override;
};

ADDON_STATUS CMyAddon::CreateInstance(int instanceType, std::string instanceID, KODI_HANDLE instance, KODI_HANDLE& addonInstance)
{
  kodi::Log(LOG_NOTICE, "Creating Timidity Audio Decoder");
  addonInstance = new CTimidityCodecAddon(instance);
  return ADDON_STATUS_OK;
}

ADDONCREATOR(CMyAddon);
