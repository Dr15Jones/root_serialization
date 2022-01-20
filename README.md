# root_serialization

This is a testing structure for doing performance experiments with various I/O packages from within an multi-threaded program. The program mimics behaviors common for
HEP data processing frameworks.

## Getting started
The CMakeLists.txt uses 3rd party packages from /cvmfs which are identical to the packages used by the CMS experiment's CMSSW_11_2_0_pre6 software release. No CMS
code is needed to build the code from this repository. CMS header files and shared libraries are needed if data files containing CMS data are being read.

To build

```
$ git clone <root_serialization URL>
$ mkdir build-root-serialization
$ cd build-root-serialization
$ cmake <path to root_serialization> \
  -DROOT_DIR=path_to_ROOT_cmake_targets \
  [-DTBB_DIR=path_to_tbb_cmake_targets] \
  [-DZSTD_DIR=path_to_zstd_cmake_targets] \
  [-DLZ4_DIR=path_to_lz4_cmake_targets]
$ make [-j N]
```

This will create the executable `threaded_io_test`.

If no cmake target exists for your installation of lz4, you can replace the cmake command above with

```
$ cmake <path to root_serialization> \
  -DCMAKE_PREFIX_PATH=path_to_lz4_install \
  -DROOT_DIR=path_to_ROOT_cmake_targets \
  [-DTBB_DIR=path_to_tbb_cmake_targets] \
  [-DZSTD_DIR=path_to_zstd_cmake_targets] \
  [-DLZ4_DIR=path_to_lz4_install]
$ make [-j N]
```

where `path_to_lz4_install` is the path to the folder holding the `include` and `lib` directories containing the files for lz4.

Note that CMake
can use some of the information from ROOT's CMake configuration to
infer the build information for TBB, zstd, and lz4.  If the ROOT
runtime is setup, one can use `-DROOT_DIR=$ROOTSYS/cmake`.

## threaded_io_test design
The testing structure has 2 customizable component types
1. `Source`s: These supply the _event_ data products used for testing.
1. `Outputer`s: These read the _event_ data products. Some Outputers also write that data out for storage.

In order to mimic the time taken to process event data, the testing structure has `Waiter`s. A `Waiter` read one _event_ data product and based on a property of that data
product calls sleep for a length proportional to that property.

The testing structure can process multiple _events_ concurrently. Each concurrent _event_ has its own set of `Waiter`s, one for each data product. The processing of 
an _event_ is handled by a `Lane`. Concurrent _events_ are then done by having multiple `Lane`s. The `Waiter`s within a `Lane` are considered to be completely 
independent and can be run concurrently.

All `Lane`s share the same `Outputer`. Therefore an `Outputer` is required to be thread safe.

For the moment, each `Lane` also has its own copy of the `Source`. That is likely to be changed in the future to better mimic the behavior of actual HEP processing
framemworks.

The way data is processed is as follows:
1. When a `Lane` is no longer processing an _event_ it requests a new one from the system. The system advances an atomic counter and tells the `Lane` to use
the _event_ associated with that index.
1. The `Lane` then passes the _event_ index to the `Source` and asks it to asynchronously retrieve the _event_ data products.
1. Once the `Source` has retrieved the data products, it signals to the `Waiter`s to run asynchronously.
1. Once each `Waiter` has finished, the system signals to the `Outputer` that the particular _event_ data product is available for the `Outputer`. The `Outputer`
can then asynchronously process that data product.
1. Once the `Outputer` has finished with all the data products, the system signals the `Outputer` that the _event_ has finished. The `Outputer` can then do
additional asynchronous work.
1. When the `Outputer` finishes its end of _event_ work, the `Lane` is considered to be done with that _event_ and the cycle repeats.

## Running tests
The `threaded_io_test` takes up to 5 command line arguments
```
threaded_io_test <Source configuration> [# threads[/useIMT]] [# conconcurrent events] [time scale factor] [max # events] [<Outputer configuration>]
```

1. `<Source configuration>` : which `Source` to use and any additional information needed to configure it. Options are described below.
1. `[# threads[/useIMT]]` : number of threads to use in the job. Optionally can add '/useIMT' directly after the number of threads to turn on ROOT's implicit multithreaded (IMT). 
1. `[# concurrent events]` : number of _events_ (that is `Lane`s) to use. Best if number of events is less than  or equal to number of threads.
1. `[time scale factor]` : used to convert the property of the _event_ data products into microseconds used for the sleep call. A value of 0 means no sleeping. A value less than 0 prohibits the creation of the objects which do the sleep.
1. `[<Outputer configuration>]` : optional, used to specify which `Outputer` to use and any additional information needed to configure it. The exact options are
described below.

## Available Components

### Sources

#### EmptySource
Does not generate any _event_ data products. Specify by just using its name, e.g. 
```
> threaded_io_test EmptySource 1 1 0 10
```

#### TestProductsSource
Generates a fixed set of data products for C++ types known by ROOT. Specify by just using its name, e.g.
```
> threaded_io_test TestProductsSource 1 1 0 10
```

#### ReplicatedRootSource
Reads a standard ROOT file. Each concurrent Event has its own replica of the Source to avoid the need for cross Event synchronization. In addition to its name, one needs to give the file to read, e.g.
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10
```

#### SerialRootSource
Reads a standard ROOT file. All concurrent Events share the same Source. Access to the Source is serialized for thread-safety. In addition to its name, one needs to give the file to read, e.g.
```
> threaded_io_test SerialRootSource=test.root 1 1 0 1000
```


#### RepeatingRootSource
Reads the first N events from a standard ROOT file at construction time. The deserialized data products are held in memory. Going from event to event is just a switch of the memory addresses to be used. In addition to its name, one needs to give the file to read and, optionally, the number of events to read (default is 10) and a singular TBranch to read, e.g.

```
> threaded_io_test RepeatingRootSource=test.root 1 1 0 1000
```
or
```
> threaded_io_test RepeatingRootSource=test.root:repeat=5 1 1 0 1000
```
or
```
> threaded_io_test RepeatingRootSource=test.root:repeat=5:branchToRead=ints 1 1 0 1000
```



#### ReplicatedPDSSource
Reads a _packed data streams_ format file. Each concurrent Event has its own replica of the Source to avoid the need for cross Event synchronization. In addition to its name, one needs to give the file to read, e.g.
```
> threaded_io_test ReplicatedPDSSource=test.pds 1 1 0 10
```

#### SharedPDSSource
Reads a _packed data streams_ format file. The Source is shared between the concurrent Events. Reads from the file are serialized for thread-safety while decompressing the Event and the object deserialization can proceed concurrently. In addition to its name, one needs to give the file to read, e.g.
```
> threaded_io_test SharedPDSSource=test.pds 1 1 0 10
```

### Outputers

#### DummyOutputer
Does no work. If no outputer is given, this is the one used. Specify by just using its name and optionally
the option label 'useProductReady'
```
> threaded_io_test EmptySource 1 1 0 10 DummyOutputer
```
or
```
> threaded_io_test EmptySource 1 1 0 10 DummyOutputer=useProductReady
```

#### TextDumpOutputer
Dumps the name and sizes for each data product. Specify by its name and the following optional parameters:
- perEvent: print names and sizes for each event. On by default. Allowed values are `perEvent=t`, `perEvent=f` and `perEvent` which is same as `perEvent=t`.
- summary: at end of job, print names and average size per event. Off by default. Allowed values are `summary=t`, `summary=f`, and `summary` which is same as `summary=t`.
```
> threaded_io_test EmptySource 1 1 0 10 TextDumpOutputer
```
or
```
> threaded_io_test EmptySource 1 1 0 10 TextDumpOutputer=perEvent=f:summary=t
```

#### SerializeOutputer
Uses ROOT to serialize the _event_ data products but does not store them. It prints timing statistics about the serialization. Specify by just using its name and an optional 'verbose' parameter
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10 SerializeOutputer
```
or
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10 SerializeOutputer=verbose
```

#### TestProductsOutputer
Checks that the data products match what is expected from TestProductsSource or files containing those same data products. If the results are unexpected, the program will abort. Specify by just using its name.
```
> threaded_io_test TestProductsSource 1 1 0 10 TestProductsOutputer
```

#### RootOutputer
Writes the _event_ data products into a ROOT file. Specify both the name of the Outputer and the file to write as well as many  optional parameters:
- splitLevel: split level for the branches, default 99
- compressionLevel: compression level 0-9, default 9
- compressionAlgorithm: name of compression algorithm. Allowed valued "", "ZLIB", "LZMA", "LZ4"
- basketSize: default size of all baskets, default size 16384
- treeMaxVirtualSize: Size of ROOT TTree TBasket cache. Use ROOT default if value is <0. Default -1.
- autoFlush: passed value to TTree SetAutoFlush. Use of the default value -1 means no call is made.
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10 RootOutputer=test.root
```
or
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10 RootOutputer=test.root:splitLevel=1
```


#### PDSOutputer
Writes the _event_ data products into a PDS file. Specify both the name of the Outputer and the file to write as well as compression options:
- compressionLevel: compression level. Allowed value depends on algorithm. For now ZSTD is the only one and allows values
  - 0 - 19 (negative values and values 20-22 are possible but not considered good choices by the zstandard authors)
- compressionAlgorithm: name of compression algorithm. Allowed valued "", "None", "ZSTD", "LZ4"
- serializationAlgorithm: name of a serialization algorithm. Allowed values "", "ROOT", "ROOTUnrolled" or "Unrolled". The default is "ROOT" (which is the same as ""). Both _unrolled_ names correspond to the same algorithm.
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10 PDSOutputer=test.pds
```

#### HDFOutputer
Writes the _event_ data products into a HDF file. Specify both the name of the Outputer and the file to write as well as the number of events to _batch_ together when writing::
- batchSize: number of events to batch together before writing out to the file. Default is 2.
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10 HDFOutputer=test.hdf
```
or
```
> threaded_io_test ReplicatedRootSource=test.root 1 1 0 10 HDFOutputer=test.hdf:batchSize=10
```

## unroll_test

The _unroll_test_ executable is meant to allow testing of the unrolled serialization process and allow comparison of object serialization sizes with respect to ROOT's standard serialization. The executable takes the following command line arguments

unroll_test [-g] [-s] [list of class names]

- -g : turns on ROOT verbose debugging output
- -s : skips running the built in test cases
- [list of class names] : names of C++ classes with ROOT dictionaries. The executable will perform serialization/deserialization on defaultly constructed instances of these classes and report the bytes needed for storage.