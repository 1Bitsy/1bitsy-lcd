#ifndef UTIL_included
#define UTIL_included

#define MIN(a, b) ({                                                    \
                      __typeof__ (a) a_ = (a);                          \
                      __typeof__ (b) b_ = (b);                          \
                      a_ < b_ ? a_ : b_;                                \
                  })

#define MAX(c, d) ({                                                    \
                      __typeof__ (c) c_ = (c);                          \
                      __typeof__ (d) d_ = (d);                          \
                      c_ > d_ ? c_ : d_;                                \
                  })

#define ABS(e)    ({                                                    \
                      __typeof__ (e) e_ = (e);                          \
                      e_ < 0 ? -e_ : e_;                                \
                  })

#define SIGN(f)   ({                                                    \
                      __typeof__ (f) f_ = (f);                          \
                      f_ < 0 ? -1 : f_ > 0 ? +1 : 0;                    \
                  })

#endif /* !UTIL_included */


