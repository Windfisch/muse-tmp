/*
  ZynAddSubFX - a software synthesizer

  main.c  -  Main file of the synthesizer
  Copyright (C) 2002-2005 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License 
  as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (version 2) for more details.

  You should have received a copy of the GNU General Public License (version 2)
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "Misc/Master.h"
#include "Misc/Util.h"
#include "MasterUI.h"

int swaplr = 0;   //1 for left-right swapping

//=========================================================
//    MESS interface
//=========================================================

#include "synti/libsynti/mess.h"
int instances = -1;

//---------------------------------------------------------
//   Zynadd
//---------------------------------------------------------

class Zynadd : public Mess {
      virtual void process(float** buffer, int offset, int n);
      virtual bool processEvent(const MidiEvent&);
      virtual void getInitData(int*, const unsigned char**) const;

   public:
      int Pexitprogram;
	MasterUI* ui;
      Master* vmaster;
	pthread_t thr;

      Zynadd();
      ~Zynadd();
      };

void* guiThread(void *arg)
      {
      Zynadd* z = (Zynadd *) arg;

      z->ui = new MasterUI(z->vmaster, &z->Pexitprogram);

      z->ui->showUI();
      while (z->Pexitprogram == 0) 
            Fl::wait(0.01);

      delete(z->ui);
      Fl::wait(0.01);

      pthread_exit(0);
      return(0);
      }

//---------------------------------------------------------
//   Zynadd
//---------------------------------------------------------

Zynadd::Zynadd() : Mess(2)
      {
      instances++;
      swaplr            = config.cfg.SwapStereo;
      Pexitprogram      = 0;

      vmaster           = new Master();
      vmaster->swaplr = swaplr;
      pthread_create(&thr, NULL, guiThread, this);
      }

//---------------------------------------------------------
//   ~Zynadd
//---------------------------------------------------------

Zynadd::~Zynadd()
      {
      Pexitprogram = 1;
      sleep(2);               //wait the thread to finish
      instances--;
      }

//---------------------------------------------------------
//    getInitData
//---------------------------------------------------------

void Zynadd::getInitData(int* n, const unsigned char** data) const
      {
      *n = vmaster->getalldata((char **)data);
      }

//---------------------------------------------------------
//   process
//    synthesize n samples into buffer+offset
//---------------------------------------------------------

void Zynadd::process(float** outputs, int offset, int n)
      {
      float* outl = outputs[0] + offset;
      float* outr = outputs[1] + offset;
      pthread_mutex_lock(&vmaster->mutex);
      vmaster->GetAudioOutSamples(n, outl, outr);
      pthread_mutex_unlock(&vmaster->mutex);
      }

//---------------------------------------------------------
//    processEvent
//---------------------------------------------------------

bool Zynadd::processEvent(const MidiEvent& e)
      {
      int ch = e.channel();
	pthread_mutex_lock(&vmaster->mutex);
      switch(e.type()) {
            case 0x80:  // note off
	            vmaster->NoteOff(ch, e.dataA());
                  break;
            case 0x90:  // note on
	            if (e.dataB() == 0) 
                        vmaster->NoteOff(ch, e.dataA());
		      else 
                        vmaster->NoteOn(ch, e.dataA(), e.dataB());
		      break;
	      case 0xb0:  // controller
                  switch(e.dataA()) {
                        case 0x4000:
	                        vmaster->SetController(ch, C_pitchwheel, e.dataB());
                              break;
                        default:
                              {
                              int ctl;
                              switch (e.dataA()) {
	                              case 1:    ctl = C_modwheel; break;
                           	      case 7:    ctl = C_volume; break;
                           	      case 10:   ctl = C_panning; break;
                           	      case 11:   ctl = C_expression; break;
                           	      case 64:   ctl = C_sustain; break;
                           	      case 65:   ctl = C_portamento; break;
                           	      case 71:   ctl = C_filterq; break;
                           	      case 74:   ctl = C_filtercutoff; break;
                           	      case 75:   ctl = C_bandwidth; break;
                           	      case 76:   ctl = C_fmamp; break;
                           	      case 77:   ctl = C_resonance_center; break;
                           	      case 78:   ctl = C_resonance_bandwidth; break;
                           	      case 120:  ctl = C_allsoundsoff; break;
                           	      case 121:  ctl = C_resetallcontrollers; break;
                           	      case 123:  ctl = C_allnotesoff; break;
                           	      case 0x06: ctl = C_dataentryhi; break;
                           	      case 0x26: ctl = C_dataentrylo; break;
                           	      case 99:   ctl = C_nrpnhi; break;
                           	      case 98:   ctl = C_nrpnlo; break;
                           	      default:   ctl = C_NULL; break;
                           	      }
	      	            vmaster->SetController(ch, ctl, e.dataB());
                              }
                              break;
                        }
                  break;

            case 0xf0:
	            pthread_mutex_unlock(&vmaster->mutex);
                  vmaster->putalldata((char*)e.data(), e.len());
	            pthread_mutex_lock(&vmaster->mutex);
                  break;
            }
	pthread_mutex_unlock(&vmaster->mutex);
      return false;
      }

//---------------------------------------------------------
//   instantiate
//---------------------------------------------------------

class QWidget;

static Mess* instantiate(int sr, QWidget*, const char*)
      {
      if (instances == -1) {
            config.init();
	      instances = 0;
            srand(time(0));
            SOUND_BUFFER_SIZE = 256;
            OSCIL_SIZE        = 512;      // config.cfg.OscilSize;
            SAMPLE_RATE       = sr;
            denormalkillbuf = new REALTYPE [SOUND_BUFFER_SIZE];
            for (int i = 0; i < SOUND_BUFFER_SIZE; i++) 
                  denormalkillbuf[i] = (RND - 0.5) * 1e-16;
    
            OscilGen::tmpsmps = new REALTYPE[OSCIL_SIZE];
                  newFFTFREQS(&OscilGen::outoscilFFTfreqs,OSCIL_SIZE/2);
            }
      if (instances != 0) 
            return 0;   //don't allow multiple instances

      Zynadd* sintetizator = new Zynadd();
      sintetizator->setSampleRate(sr);
      return sintetizator;
      }

extern "C" {
      static MESS descriptor = {
            "Zynaddsubfx",
            "Zynaddsubfx Software Synthesizer",
            "0.1",      // version string
            MESS_MAJOR_VERSION, MESS_MINOR_VERSION,
            instantiate
            };

      // We must compile with -fvisibility=hidden to avoid namespace
      // conflicts with global variables.
      // Only visible symbol is "mess_descriptor".
      // (TODO: all plugins should be compiled this way)

      __attribute__ ((visibility("default")))
         const MESS* mess_descriptor() { return &descriptor; }
      }
