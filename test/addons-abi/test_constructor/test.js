'use strict';
require('../../common');
var assert = require('assert');

// Testing api calls for a constructor that defines properties
var TestConstructor = require('./build/Release/test_constructor');
var test_object = new TestConstructor();

assert.equal(test_object.echo('hello'), 'hello');

test_object.readwriteValue = 1;
assert.equal(test_object.readwriteValue, 1);
test_object.readwriteValue = 2;
assert.equal(test_object.readwriteValue, 2);

assert.throws(() => { test_object.readonlyValue = 3; });

assert.ok(test_object.hiddenValue);

// All properties except 'hiddenValue' should be enumerable.
var propertyNames = [];
for (var name in test_object) {
  propertyNames.push(name);
}
assert.ok(propertyNames.indexOf('echo') >= 0);
assert.ok(propertyNames.indexOf('readwriteValue') >= 0);
assert.ok(propertyNames.indexOf('readonlyValue') >= 0);
assert.ok(propertyNames.indexOf('hiddenValue') < 0);
