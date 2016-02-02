
## cppy2 -- embed python into your c++ app in 10 minutes

### Minimalistic library for embedding python 2 in C++ application.

No additional dependencies required.
Linux, Windows platforms supported
Qt5 dependency is optional (QString unicode converters)

### Functionality

* Convenient rererence-counted holder for PyObject*
* Simple wrapper over init/shutdown of interpreter in 1 line of code
* Manage GIL with scoped lock/unlock guards
* Translate exceptions from Python to to C++ layer
* C++ abstractions for list, dict and numpy.ndarray

### Build hints

Go straight:

```
cd cppy2 && mkdir build && cd build
cmake .. && make && ./tests
```

Use `cmake -DCMAKE_PREFIX_PATH=/path/to/Qt5.5.1`
to build with custom Qt version instead of system-default.


### (c) 2015 Dennis Shilko
