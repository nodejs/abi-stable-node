'use strict';
require('../../common');
var assert = require('assert');

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
