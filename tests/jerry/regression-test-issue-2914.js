

var func = function (number) {
    "use strict";
    number.text = "";
};

try {
    func(5);
    throw Error("Should throw TypeError");
} catch(ex) {
    if (!ex instanceof TypeError) {
        throw ex;
    }
}
