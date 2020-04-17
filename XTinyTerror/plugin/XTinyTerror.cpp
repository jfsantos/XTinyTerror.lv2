/*
 * Copyright (C) 2014 Guitarix project MOD project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * --------------------------------------------------------------------------
 */


#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>
#include <unistd.h>

///////////////////////// MACRO SUPPORT ////////////////////////////////

#define __rt_func __attribute__((section(".rt.text")))
#define __rt_data __attribute__((section(".rt.data")))

///////////////////////// FAUST SUPPORT ////////////////////////////////

#define FAUSTFLOAT float
#ifndef N_
#define N_(String) (String)
#endif
#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

#define always_inline inline __attribute__((always_inline))

#ifndef signbit
#define signbit(x) std::signbit(x)
#endif

template<class T> inline T mydsp_faustpower2_f(T x) {return (x * x);}
template<class T> inline T mydsp_faustpower3_f(T x) {return ((x * x) * x);}
template<class T> inline T mydsp_faustpower4_f(T x) {return (((x * x) * x) * x);}
template<class T> inline T mydsp_faustpower5_f(T x) {return ((((x * x) * x) * x) * x);}
template<class T> inline T mydsp_faustpower6_f(T x) {return (((((x * x) * x) * x) * x) * x);}

////////////////////////////// LOCAL INCLUDES //////////////////////////

#include "XTinyTerror.h"        // define struct PortIndex
#include "tinyterror.cc"    // dsp class generated by faust -> dsp2cc

////////////////////////////// PLUG-IN CLASS ///////////////////////////

namespace tinyterror {

class Gx_tinyterror_
{
private:
  // pointer to buffer
  float*          output;
  float*          input;
  // pointer to dsp class
  Dsp*      tinyterror;

  // bypass ramping
  float*          bypass;
  uint32_t        bypass_;
 
  bool            needs_ramp_down;
  bool            needs_ramp_up;
  float           ramp_down;
  float           ramp_up;
  float           ramp_up_step;
  float           ramp_down_step;
  bool            bypassed;

  // private functions
  inline void run_dsp_(uint32_t n_samples);
  inline void connect_(uint32_t port,void* data);
  inline void init_dsp_(uint32_t rate);
  inline void connect_all__ports(uint32_t port, void* data);
  inline void activate_f();
  inline void clean_up();
  inline void deactivate_f();

public:
  // LV2 Descriptor
  static const LV2_Descriptor descriptor;
  // static wrapper to private functions
  static void deactivate(LV2_Handle instance);
  static void cleanup(LV2_Handle instance);
  static void run(LV2_Handle instance, uint32_t n_samples);
  static void activate(LV2_Handle instance);
  static void connect_port(LV2_Handle instance, uint32_t port, void* data);
  static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                                double rate, const char* bundle_path,
                                const LV2_Feature* const* features);
  Gx_tinyterror_();
  ~Gx_tinyterror_();
};

// constructor
Gx_tinyterror_::Gx_tinyterror_() :
  output(NULL),
  input(NULL),
  tinyterror(tinyterror::plugin()),
  bypass(0),
  bypass_(2),
  needs_ramp_down(false),
  needs_ramp_up(false),
  bypassed(false) {};

// destructor
Gx_tinyterror_::~Gx_tinyterror_()
{
  // delete DSP class
  tinyterror->del_instance(tinyterror);
};

///////////////////////// PRIVATE CLASS  FUNCTIONS /////////////////////

void Gx_tinyterror_::init_dsp_(uint32_t rate)
{
  // set values for internal ramping
  ramp_down_step = 32 * (256 * rate) / 48000; 
  ramp_up_step = ramp_down_step;
  ramp_down = ramp_down_step;
  ramp_up = 0.0;

  tinyterror->init_static(rate, tinyterror); // init the DSP class
}

// connect the Ports used by the plug-in class
void Gx_tinyterror_::connect_(uint32_t port,void* data)
{
  switch ((PortIndex)port)
    {
    case EFFECTS_OUTPUT:
      output = static_cast<float*>(data);
      break;
    case EFFECTS_INPUT:
      input = static_cast<float*>(data);
      break;
    case BYPASS: 
      bypass = static_cast<float*>(data); // , 0.0, 0.0, 1.0, 1.0 
      break;
    default:
      break;
    }
}

void Gx_tinyterror_::activate_f()
{
  // allocate the internal DSP mem
}

void Gx_tinyterror_::clean_up()
{
  tinyterror->clear_state_f_static(tinyterror);
  // delete the internal DSP mem
}

void Gx_tinyterror_::deactivate_f()
{
  // delete the internal DSP mem
}

void Gx_tinyterror_::run_dsp_(uint32_t n_samples)
{
  if(n_samples<1) return;
  FAUSTFLOAT buf[n_samples];
  // do inplace processing at default
  if (output != input)
    memcpy(output, input, n_samples*sizeof(float));
  // check if bypass is pressed
  if (bypass_ != static_cast<uint32_t>(*(bypass))) {
    bypass_ = static_cast<uint32_t>(*(bypass));
    if (!bypass_) {
      needs_ramp_down = true;
      needs_ramp_up = false;
    } else {
      needs_ramp_down = false;
      needs_ramp_up = true;
      bypassed = false;
    }
  }

  if (needs_ramp_down || needs_ramp_up) {
       memcpy(buf, input, n_samples*sizeof(float));
  }
  
  if (!bypassed) {
     tinyterror->compute_static(static_cast<int>(n_samples), output, output, tinyterror);
  }

  // check if ramping is needed
  if (needs_ramp_down) {
    float fade = 0;
    for (uint32_t i=0; i<n_samples; i++) {
      if (ramp_down >= 0.0) {
        --ramp_down; 
      }
      fade = max(0.0,ramp_down) /ramp_down_step ;
      output[i] = output[i] * fade + buf[i] * (1.0 - fade);
    }

    if (ramp_down <= 0.0) {
      // when ramped down, clear buffer from tinyterror class
      tinyterror->clear_state_f_static(tinyterror);
      needs_ramp_down = false;
      bypassed = true;
      ramp_down = ramp_down_step;
      ramp_up = 0.0;
    } else {
      ramp_up = ramp_down;
    }

  } else if (needs_ramp_up) {
    float fade = 0;
    for (uint32_t i=0; i<n_samples; i++) {
      if (ramp_up < ramp_up_step) {
        ++ramp_up ;
      }
      fade = min(ramp_up_step,ramp_up) /ramp_up_step ;
      output[i] = output[i] * fade + buf[i] * (1.0 - fade);
    }

    if (ramp_up >= ramp_up_step) {
      needs_ramp_up = false;
      ramp_up = 0.0;
      ramp_down = ramp_down_step;
    } else {
      ramp_down = ramp_up;
    }
  }

}

void Gx_tinyterror_::connect_all__ports(uint32_t port, void* data)
{
  // connect the Ports used by the plug-in class
  connect_(port,data); 
  // connect the Ports used by the DSP class
  tinyterror->connect_static(port,  data, tinyterror);
}

////////////////////// STATIC CLASS  FUNCTIONS  ////////////////////////

LV2_Handle 
Gx_tinyterror_::instantiate(const LV2_Descriptor* descriptor,
                            double rate, const char* bundle_path,
                            const LV2_Feature* const* features)
{
  // init the plug-in class
  Gx_tinyterror_ *self = new Gx_tinyterror_();
  if (!self) {
    return NULL;
  }

  self->init_dsp_((uint32_t)rate);

  return (LV2_Handle)self;
}

void Gx_tinyterror_::connect_port(LV2_Handle instance, 
                                   uint32_t port, void* data)
{
  // connect all ports
  static_cast<Gx_tinyterror_*>(instance)->connect_all__ports(port, data);
}

void Gx_tinyterror_::activate(LV2_Handle instance)
{
  // allocate needed mem
  static_cast<Gx_tinyterror_*>(instance)->activate_f();
}

void Gx_tinyterror_::run(LV2_Handle instance, uint32_t n_samples)
{
  // run dsp
  static_cast<Gx_tinyterror_*>(instance)->run_dsp_(n_samples);
}

void Gx_tinyterror_::deactivate(LV2_Handle instance)
{
  // free allocated mem
  static_cast<Gx_tinyterror_*>(instance)->deactivate_f();
}

void Gx_tinyterror_::cleanup(LV2_Handle instance)
{
  // well, clean up after us
  Gx_tinyterror_* self = static_cast<Gx_tinyterror_*>(instance);
  self->clean_up();
  delete self;
}

const LV2_Descriptor Gx_tinyterror_::descriptor =
{
  PLUGIN_URI "#_tinyterror_",
  Gx_tinyterror_::instantiate,
  Gx_tinyterror_::connect_port,
  Gx_tinyterror_::activate,
  Gx_tinyterror_::run,
  Gx_tinyterror_::deactivate,
  Gx_tinyterror_::cleanup,
  NULL
};


} // end namespace tinyterror

////////////////////////// LV2 SYMBOL EXPORT ///////////////////////////

extern "C"
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
  switch (index)
    {
    case 0:
      return &tinyterror::Gx_tinyterror_::descriptor;
    default:
      return NULL;
    }
}

///////////////////////////// FIN //////////////////////////////////////
