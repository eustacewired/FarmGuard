#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class TinyMLBrain {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        if (x[1] <= 50.0061092376709) {
                            if (x[1] <= 34.86375045776367) {
                                if (x[0] <= 35.56812858581543) {
                                    return 3;
                                }

                                else {
                                    if (x[2] <= 61.16319465637207) {
                                        return 2;
                                    }

                                    else {
                                        return 3;
                                    }
                                }
                            }

                            else {
                                if (x[2] <= 64.79772853851318) {
                                    return 1;
                                }

                                else {
                                    return 3;
                                }
                            }
                        }

                        else {
                            if (x[2] <= 34.10647964477539) {
                                if (x[1] <= 57.50921058654785) {
                                    return 1;
                                }

                                else {
                                    return 3;
                                }
                            }

                            else {
                                if (x[2] <= 89.92671585083008) {
                                    if (x[0] <= -38.43817329406738) {
                                        return 3;
                                    }

                                    else {
                                        return 0;
                                    }
                                }

                                else {
                                    return 3;
                                }
                            }
                        }
                    }

                protected:
                };
            }
        }
    }