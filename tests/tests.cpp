#include <cppy2/cppy2.hpp>
#include <cppy2/cppy2numpy.hpp>

#include <QDebug>
#include <iostream>

void testInjectSimpleTypes() {
  cppy2::Main().injectVar<int>("a", 2);
  cppy2::Main().injectVar<int>("b", 2);
  cppy2::exec("assert a + b == 4");
}

void TestInjectNumpyArray() {

  cppy2::importNumpy();
  cppy2::exec("import numpy");
  cppy2::exec("print 'hello numpy', numpy.version.full_version");

  // create ndarray on a c-side
  double data[2] = {3.14, 42};
  // create copy
  cppy2::NDArray<double> a(data, 2, 1);
  // wrap data without copying
  cppy2::NDArray<double> b;
  b.wrap(data, 2, 1);
  assert(a(1, 0) == data[1] && "expect data in ndarray");
  assert(b(1, 0) == data[1] && "expect data in ndarray");

  // inject into python __main__ namespace
  cppy2::Main().inject("a", a);
  cppy2::Main().inject("b", b);
  cppy2::exec("print a");
  cppy2::exec("assert type(a) == numpy.ndarray, 'expect injected instance'");
  cppy2::exec("assert numpy.all(a == b), 'expect data'");

  // modify b from python
  cppy2::exec("b[0] = 100500");
  assert(b(0, 0) == 100500 && "expect data is modified");
}

int main(int argc, char *argv[]) {

  try {
    // create interpreter
    cppy2::PythonVM instance;

    // run bunch of tests
    testInjectSimpleTypes();
    TestInjectNumpyArray();

  } catch (const cppy2::PythonException& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
