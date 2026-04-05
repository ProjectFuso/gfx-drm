# Build Workflow

## Remote VM build

Use this workflow to validate the `drm` and `vmwgfx` modules on the illumos VM.

1. Push the current branch:

```sh
git push
```

2. Update the checkout on the VM:

```sh
ssh youkou@172.16.231.152 'cd ~/gfx-drm && git pull'
```

3. Build and install the DRM core and `vmwgfx` modules on the VM:

```sh
ssh youkou@172.16.231.152 'cd ~/gfx-drm && /usr/bin/ksh93 tools/bldenv -d myenv.sh "cd usr/src/uts/intel/drm && make install && cd ../vmwgfx && make install"'
```

## Notes

- This builds in the VM instead of the local workspace, so compiler output there
  is the source of truth for illumos integration issues.
- The build currently uses the `youkou@172.16.231.152` VM and the checkout at
  `~/gfx-drm`.
- A successful run ends with installation into the VM proto area, for example:
  `.../proto/root_i386/kernel/drv/amd64/vmwgfx`
