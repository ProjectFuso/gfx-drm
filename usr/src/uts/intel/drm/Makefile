#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
#	This makefile drives the production of the DRM (Direct Rendering
#	Manager) common misc module.
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= drm
OBJECTS		= $(DRM_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DRM_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

CMN_DRM		= $(UTSBASE)/common/io/drm

# Include paths: illumos compat headers + system includes
INC_PATH	+= -I$(CMN_DRM)/include
INC_PATH	+= -I$(CMN_DRM)/include/linux
INC_PATH	+= -I$(CMN_DRM)/include/drm
INC_PATH	+= -I$(CMN_DRM)/include/uapi
INC_PATH	+= -I$(CMN_DRM)/include/uapi/drm

# Dependency
LDFLAGS		+= -dy -Nmisc/agpmaster -Nmisc/gfx_private

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel
include $(UTSBASE)/common/io/drm/Makefile.mod

# Suppress common upstream warnings
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-but-set-variable
CERRWARN	+= -_gcc=-Wno-missing-field-initializers
CERRWARN	+= -_gcc=-Wno-sign-compare
CERRWARN	+= -_gcc=-Wno-implicit-fallthrough
CERRWARN	+= -_gcc=-Wno-address-of-packed-member

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
# Compile rules for top-level DRM sources
#
$(OBJS_DIR)/%.o:		$(CMN_DRM)/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(LINTS_DIR)/%.ln:		$(CMN_DRM)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

#
# Compile rules for display/ subdirectory (objects prefixed with display_)
#
$(OBJS_DIR)/display_%.o:	$(CMN_DRM)/display/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(LINTS_DIR)/display_%.ln:	$(CMN_DRM)/display/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

#
# Compile rules for ttm/ subdirectory (objects prefixed with ttm_)
#
$(OBJS_DIR)/ttm_%.o:		$(CMN_DRM)/ttm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(LINTS_DIR)/ttm_%.ln:		$(CMN_DRM)/ttm/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

#
# Compile rules for scheduler/ subdirectory (objects prefixed with sched_)
#
$(OBJS_DIR)/sched_%.o:		$(CMN_DRM)/scheduler/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(LINTS_DIR)/sched_%.ln:	$(CMN_DRM)/scheduler/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ
