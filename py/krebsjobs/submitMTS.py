#!/usr/bin/env python2
# -*- coding: utf-8 -*-
'''
This file is part of tumorcode project.
(http://www.uni-saarland.de/fak7/rieger/homepage/research/tumor/tumor.html)

Copyright (C) 2016  Michael Welter and Thierry Fredrich

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
'''

if __name__ == '__main__':
  import os.path, sys
  sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..'))
  
import os, sys
from os.path import join, basename, dirname, splitext, abspath
sys.path.append(abspath(join(dirname(__file__),'..')))
import qsub
import glob
import h5py
import numpy as np
import krebsutils
import krebs
import krebs.tumors
from copy import deepcopy
from dicttoinfo import dicttoinfo, Vec
from myutils import f2s

#exe = krebsutils.get_full_tumor_executable_path('tum-only-vessels')
dstdir = os.getcwd()

#note: according to doeme et al. the tumor radius growth should be around 0.7 mu m / h

import krebsjobs
import krebsjobs.parameters.parameterSetsMTS as parameterSets
from krebsjobs.parameters import parameterSetsO2

#from submitAdaption import create_auto_dicts

from krebsutils import typelist


def getDomainSizeFromVesselFile(fn):
  with h5py.File(fn, 'r') as f:
    ld = krebsutils.read_lattice_data_from_hdf(krebsutils.find_lattice_group_(f['vessels']))
    size = np.amax(ld.GetWorldSize())
  # longest axis times the lattice spacing
  return size


def MakeVesselFilenamePart(fn):
  with h5py.File(fn, mode='a') as f:
    if 'parameters' in f:
      if 'MESSAGE' in f['parameters'].attrs:
        msg = f['parameters'].attrs['MESSAGE']
        ensemble_index = f['parameters'].attrs['ENSEMBLE_INDEX']
        if msg.startswith('vessels-'): msg=msg[len('vessels-'):]
    if 'msg' not in locals():
      msg = "hm"
      ensemble_index = 1
      f['parameters'].attrs['MESSAGE'] = msg 
      f['parameters'].attrs['ENSEMBLE_INDEX'] = ensemble_index
    
  name = '%s-sample%02i' % (msg, ensemble_index)
  return name


def run(vessel_fn, name, paramSet, mem, days):
  
  name, paramSet = krebsjobs.PrepareConfigurationForSubmission(vessel_fn, name, 'fakeTumMTS', paramSet)
  configstr = dicttoinfo(paramSet)
  config_file_name = '%s.info' % paramSet['fn_out']
  with open(config_file_name, 'w') as f:
    f.write(configstr)
    
  #o2params = getattr(parameterSetsO2, "breastv3")
  qsub.submit(qsub.func(krebs.tumors.run_faketum_mts, config_file_name),
                            name = 'job_'+name,
                            mem = mem,
                            days = days,
                            num_cpus = paramSet['num_threads'],
                            change_cwd = True)


if __name__ == '__main__':
  import argparse
  parser = argparse.ArgumentParser(description='Compute Fake tumor. Vessels are needed for that')  
  parser.add_argument('tumParamSet', help='Valid configuration are found in /py/krebsjobs/parameters/fparameterSetsFakeTumor.py')
  parser.add_argument('vesselFileNames', nargs='*', type=argparse.FileType('r'), default=sys.stdin, help='Vessel file to calculate')

  goodArguments, otherArguments = parser.parse_known_args()
  qsub.parse_args(otherArguments)
  
  tumorParameterName = goodArguments.tumParamSet
  #create filename due to former standards
  filenames=[]
  for fn in goodArguments.vesselFileNames:
    filenames.append(fn.name)
    
  try:
    if not (tumorParameterName in dir(parameterSets)) and (not 'auto' in tumorParameterName):
        raise AssertionError('Unknown parameter set %s!' % tumorParameterName)
    for fn in filenames:
        if not os.path.isfile(fn):
            raise AssertionError('The file %s is not present!'%fn)
  except Exception, e:
    print e.message
    sys.exit(-1)

  
  factory = getattr(parameterSets, tumorParameterName)
  for fn in filenames:
    #run(fn, tumorParameterName, factory, '4GB', 2.)
    run(fn, tumorParameterName, factory, '2GB', 5.)
  
        
