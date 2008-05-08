# -*- Mode: Python -*-
# vi:si:et:sw=4:sts=4:ts=4
#
# gst-python
# Copyright (C) 2002 David I. Lehn
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
# 
# Author: David I. Lehn <dlehn@users.sourceforge.net>

__gstltihooks_used__ = False
try:
    import gstltihooks
    __gstltihooks_used__ = True
except:
    pass

import sys

# we always require 2.0 of pygtk; so if pygtk is not imported anywhere
# yet, we import pygtk here and .require
if 'gobject' not in sys.modules:
    import pygtk
    pygtk.require('2.0')

class Value:
    def __init__(self, type):
        assert type in ('fourcc', 'intrange', 'doublerange', 'fractionrange', 'fraction')
        self.type = type

class Fourcc(Value):
    def __init__(self, string):
        Value.__init__(self, 'fourcc')
        self.fourcc = string
    def __repr__(self):
        return '<gst.Fourcc %s>' % self.fourcc

class IntRange(Value):
    def __init__(self, low, high):
        Value.__init__(self, 'intrange')
        self.low = low
        self.high = high
    def __repr__(self):
        return '<gst.IntRange [%d, %d]>' % (self.low, self.high)

class DoubleRange(Value):
    def __init__(self, low, high):
        Value.__init__(self, 'doublerange')
        self.low = low
        self.high = high
    def __repr__(self):
        return '<gst.DoubleRange [%f, %f]>' % (self.low, self.high)

class FractionRange(Value):
    def __init__(self, low, high):
        Value.__init__(self, 'fractionrange')
        self.low = low
        self.high = high
    def __repr__(self):
        return '<gst.FractionRange [%d/%d, %d/%d]>' % (self.low.num,
                                                       self.low.denom,
                                                       self.high.num,
                                                       self.high.denom)

class Fraction(Value):
    def __init__(self, num, denom=1):
        Value.__init__(self, 'fraction')
        self.num = num
        self.denom = denom

    def __repr__(self):
        return '<gst.Fraction %d/%d>' % (self.num, self.denom)

    def __eq__(self, other):
        if isinstance(other, Fraction):
            return self.num * other.denom == other.num * self.denom
        return False

    def __ne__(self, other):
        return not self.__eq__(other)

    def __mul__(self, other):
        if isinstance(other, Fraction):
            return Fraction(self.num * other.num,
                            self.denom * other.denom)
        elif isinstance(other, int):
            return Fraction(self.num * other, self.denom)
        raise TypeError

    __rmul__ = __mul__

    def __div__(self, other):
        if isinstance(other, Fraction):
            return Fraction(self.num * other.denom,
                            self.denom * other.num)
        elif isinstance(other, int):
            return Fraction(self.num, self.denom * other)
        return TypeError

    def __rdiv__(self, other):
        if isinstance(other, int):
            return Fraction(self.denom * other, self.num)
        return TypeError

    def __float__(self):
        return float(self.num) / float(self.denom)


import sys
dlsave = sys.getdlopenflags()
try:
    from DLFCN import RTLD_GLOBAL, RTLD_LAZY
except ImportError:
    RTLD_GLOBAL = -1
    RTLD_LAZY = -1
    import os
    osname = os.uname()[0]
    if osname == 'Linux' or osname == 'SunOS' or osname == 'FreeBSD':
        RTLD_GLOBAL = 0x100
        RTLD_LAZY = 0x1
    elif osname == 'Darwin':
        RTLD_GLOBAL = 0x8
        RTLD_LAZY = 0x1
    del os
except:
    RTLD_GLOBAL = -1
    RTLD_LAZY = -1

if RTLD_GLOBAL != -1 and RTLD_LAZY != -1:
    sys.setdlopenflags(RTLD_LAZY | RTLD_GLOBAL)
    from _gst import *
    import interfaces
    try:
        import libxml2
    except:
        pass

import gobject
from _gst import _install_element_meta

_GstElementBaseMeta = getattr(gobject, 'GObjectMeta', type)
class _GstElementMeta(_GstElementBaseMeta):
    __call__ = element_factory_make
_install_element_meta(_GstElementMeta)

version = get_gst_version

sys.setdlopenflags(dlsave)
del sys

# Fixes for API cleanups that would cause an API breakage.
# See #446674

import warnings
if locals().has_key("parse_bin_from_description"):
    def gst_parse_bin_from_description(*args, **kwargs):
        warnings.warn("gst_parse_bin_from_description() is deprecated, please use parse_bin_from_description instead",
                      DeprecationWarning)
        return parse_bin_from_description(*args, **kwargs)

if locals().has_key("message_new_buffering"):
    def gst_message_new_buffering(*args, **kwargs):
        warnings.warn("gst_message_new_buffering() is deprecated, please use message_new_buffering() instead",
                      DeprecationWarning)
        return message_new_buffering(*args, **kwargs)

# this restores previously installed importhooks, so we don't interfere
# with other people's module importers
# it also clears out the module completely as if it were never loaded,
# so that if anyone else imports gstltihooks the hooks get installed
if __gstltihooks_used__:
    gstltihooks.uninstall()
    __gstltihooks_used__ = False
    del gstltihooks
    import sys
    del sys.modules['gstltihooks']

