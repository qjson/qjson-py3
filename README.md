# qjson to json converter

[json](https://www.json.org) is a very popular data encoding with a good support in many 
programming languages. It may thus seam a good idea to use it for manually managed 
data like configuration files. Unfortunately, json is not very convenient for such 
application because every string must be quoted and elements must be separated by commas. 
It is easy to parse by a program and to verify that it is correct, but it’s not connvenient
to write. 

For this reason, different alternatives to json have been proposed which are more human 
friendly like [yaml] or [hjson](https://hjson.github.io/) for instance. 

qjson is inspired by hjson by being a human readable and extended json. The difference
between qjson and hjson is that qjson extends its functionality and relax some rules.

Here is a list of qjson text properties:

- comments of the form //...  #... or /*...*/
- commas between array values and object members are optional 
- double quote, single quote and quoteless strings
- non-breaking space is considered as a white space character
- newline is \n or \r\n, report \r alone or \n\r as an error
- numbers are integer, floating point, hexadecimal, octal or binary
- numbers may contain underscore '_' as separator
- numbers may be simple mathematical expression with parenthesis
- member identifiers may be quoteless strings including spaces
- the newline type in multiline string is explicitely specified
- backspace and form feed controls are invalid characters except
  in /*...*/ comments or multiline strings
- time durations expressed with w, d, h, m, s suffix are converted to seconds
- time specified in ISO format is converted to UTC time is seconds

## Usage 

Install the qjson to json converter package with the command

`python3 -m pip install qjson2json`

Onced installed, it can be used.

```
$python3
>>> import qjson2json
>>> qjson2json.decode('a:b')
'{"a":"b"}'

## Reliability

qjson2json is a python extension using the C library qjson-c. 
qjson-c is a direct translation of qjson-go that has been extensively tested 
with manualy defined tests (100% coverage). AFL fuzzing was run on qjson-c
for many months without finding a simple hang or crash. For this fuzzing test,
the json text produced, when the input was valid, was checked with json-c to
detect invalid json. All json output in the tests are valid json.

This code should thus not crash or hang your program, and json output is valid.
What could not be extensively and automatically tested is if the output is 
the expected output. It was only verified in the manual tests of qjson-go. 

For bug reports in this qjson2json package, fill an issue in 
[the qjson-py3 project](https://github.com/chmike/qjson-py3). 

## Contributing

qjson is a recently developped package. It is thus a good time to 
suggest changes or extensions to the syntax since the user base is very
small. 

For suggestions or problems relative to syntax, fill an issue in the 
[qjson-syntax project](http://github.com/qjson/qjson-syntax).

Any contribution is welcome. 

## License

The licences is the 3-BSD clause. 
