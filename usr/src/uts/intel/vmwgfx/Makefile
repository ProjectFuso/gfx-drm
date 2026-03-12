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
# Copyright 2024 illumos vmwgfx port contributors.
# Use is subject to license terms.
#

#
#	This makefile drives the production of the VMware vmwgfx DRM driver
#	module.
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= vmwgfx
OBJECTS		= $(VMWGFX_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(VMWGFX_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

CMN_DRM		= $(UTSBASE)/common/io/drm

# Include paths: illumos compat headers + system includes + vmwgfx internals
INC_PATH	+= -I$(CMN_DRM)/include
INC_PATH	+= -I$(CMN_DRM)/include/linux
INC_PATH	+= -I$(CMN_DRM)/include/drm
INC_PATH	+= -I$(CMN_DRM)/include/uapi
INC_PATH	+= -I$(CMN_DRM)/include/uapi/drm
INC_PATH	+= -I$(CMN_DRM)/vmwgfx

# Dependencies
LDFLAGS		+= -dy -Nmisc/drm -Nmisc/agpmaster -Nmisc/gfx_private

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel
include $(CMN_DRM)/vmwgfx/Makefile.mod

# illumos/amd64: enable the CONFIG_X86 path in DRM/vmwgfx source
CPPFLAGS	+= -DCONFIG_X86

# Suppress common upstream warnings
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-but-set-variable
CERRWARN	+= -_gcc=-Wno-missing-field-initializers
CERRWARN	+= -_gcc=-Wno-sign-compare
CERRWARN	+= -_gcc=-Wno-implicit-fallthrough
CERRWARN	+= -_gcc=-Wno-format
CERRWARN	+= -_gcc=-Wno-maybe-uninitialized
CERRWARN	+= -_gcc=-Wno-pointer-sign

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
# Compile rules for vmwgfx sources (objects prefixed with vmw_)
#
$(OBJS_DIR)/vmw_vmwgfx_%.o:		$(CMN_DRM)/vmwgfx/vmwgfx_%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(LINTS_DIR)/vmw_vmwgfx_%.ln:		$(CMN_DRM)/vmwgfx/vmwgfx_%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

$(OBJS_DIR)/vmw_ttm_%.o:		$(CMN_DRM)/vmwgfx/ttm_%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(LINTS_DIR)/vmw_ttm_%.ln:		$(CMN_DRM)/vmwgfx/ttm_%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ
