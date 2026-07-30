#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 HDF5_ROOT SOURCE_DIR PLUGIN_DIR" >&2
    exit 2
fi

hdf5_root=$1
source_dir=$2
plugin_dir=$3
plugin="$plugin_dir/libh5lzf.dylib"
if [ -f "$plugin" ]; then
    exit 0
fi

mkdir -p "$source_dir" "$plugin_dir"
base=https://raw.githubusercontent.com/h5py/h5py/3.14.0/lzf
for file in lzf_filter.c lzf_filter.h; do
    curl --fail --location --retry 8 --silent --show-error \
        "$base/$file" -o "$source_dir/$file"
done
for file in lzf.h lzfP.h lzf_c.c lzf_d.c; do
    curl --fail --location --retry 8 --silent --show-error \
        "$base/lzf/$file" -o "$source_dir/$file"
done

cc -O3 -fPIC -dynamiclib \
    -I"$hdf5_root/include" -I"$source_dir" \
    "$source_dir/lzf_filter.c" "$source_dir/lzf_c.c" "$source_dir/lzf_d.c" \
    -L"$hdf5_root/lib" -lhdf5 -o "$plugin"
