from setuptools import setup, Extension

setup(
  ext_modules=[Extension('qjson2json',
                       ['src/qjson.c', 'src/qjsonmodule.c'],
                       depends=['src/qjson.h'],
                       include_dirs=['src'],
              )],
)
