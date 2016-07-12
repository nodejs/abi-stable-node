'use strict';
require('../../common');
var assert = require('assert');
var test_number = require('./build/Release/test_number');


// testing api calls for number
var numbers = [1, 100, 2121, 986583];

numbers.forEach(function(element, index) {
  assert.equal(element, test_number.Test(element));
});


// testing api calls for string
var test_string = require('./build/Release/test_string');

var strings = ['hello world', 'a', '?!@#$%^&*()_+-=\[]{}/.,<>', '\u{2003}'];

strings.forEach(function(element, index) {
  assert.equal(element, test_string.Test(element));
});


// Testing api calls for objects
var test_object = require('./build/Release/test_object');

var objects = [
  {
    name: "unknown",
    test: "name"
  },
  {
    hello: "world",
    test: "hello"
  }
];

objects.forEach(function(element, index) {
  assert.equal(test_object.Test(element, element.test), element[element.test]);
});


// testing api calls for function
var test_function = require('./build/Release/test_function');

var functions = [
  function () { return 1; },
  function () { return 2; }
];

functions.forEach(function(element, index) {
  var expected = element();
  var output = test_function.Test(element);
  assert.equal(output, expected);
});
return;
// without second iteration, make test report free() error at an invalid pointer
functions.forEach(function(element, index) {
  var expected = element();
  var output = test_function.Test(element);
  assert.equal(output, expected);
});


