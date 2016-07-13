'use strict';
require('../../common');
var assert = require('assert');

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
