'use strict';
var common = require('../../common');
var assert = require('assert');
var test_number = require(`./build/${common.buildType}/test_number`);


// testing api calls for number
assert.equal(0, test_number.Test(0));
assert.equal(1, test_number.Test(1));
assert.equal(-1, test_number.Test(-1));
assert.equal(100, test_number.Test(100));
assert.equal(2121, test_number.Test(2121));
assert.equal(-1233, test_number.Test(-1233));
assert.equal(986583, test_number.Test(986583));
assert.equal(-976675, test_number.Test(-976675));

var num1 = 98765432213456789876546896323445679887645323232436587988766545658;
assert.equal(num1, test_number.Test(num1));

var num2 = -4350987086545760976737453646576078997096876957864353245245769809;
assert.equal(num2, test_number.Test(num2));

var num3 = Number.MAX_SAFE_INTEGER;
assert.equal(num3, test_number.Test(num3));

var num4 = Number.MAX_SAFE_INTEGER + 10;
assert.equal(num4, test_number.Test(num4));

var num5 = Number.MAX_VALUE;
assert.equal(num5, test_number.Test(num5));

var num6 = Number.MAX_VALUE + 10;
assert.equal(num6, test_number.Test(num6));

var num7 = Number.POSITIVE_INFINITY;
assert.equal(num7, test_number.Test(num7));

var num8 = Number.NEGATIVE_INFINITY;
assert.equal(num8, test_number.Test(num8));
