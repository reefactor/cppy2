
# cppy2 
## Embed python into your c++ app in 10 minutes
### Minimalistic library for embedding python 2 in C++ application

No additional dependencies required.
Linux, Windows platforms supported
Qt5 dependency is optional (QString unicode converters)

### Functionality

* Hold rererence-counted smart pointer for PyObject*
* Manage init/shutdown of interpreter in 1 line of code
* Manage GIL with scoped lock/unlock guards
* Translate exceptions from Python to to C++ layer
* Nice C++ abstractions for Python native types list, dict and numpy.ndarray

### Build hints

Go straight:

```
cd cppy2 && mkdir build && cd build
cmake .. && make && ./tests
```

Use `cmake -DCMAKE_PREFIX_PATH=/path/to/Qt5.5.1`
to build with custom Qt version instead of system-default.


### MIT License. Feel free to use it.
