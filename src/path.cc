#include "uv.h"

#undef ERROR

#include "Rcpp.h"
#include "error.h"
#include "utils.h"
#include <libgen.h>

using namespace Rcpp;

// [[Rcpp::export]]
CharacterVector realize_(CharacterVector path) {
  CharacterVector out = CharacterVector(path.size());

  for (R_xlen_t i = 0; i < Rf_xlength(out); ++i) {
    uv_fs_t req;
    const char* p = CHAR(STRING_ELT(path, i));
    uv_fs_realpath(uv_default_loop(), &req, p, NULL);
    stop_for_error(req, "Failed to realize '%s'", p);
    SET_STRING_ELT(out, i, Rf_mkChar((const char*)req.ptr));
    uv_fs_req_cleanup(&req);
  }
  return out;
}
// [[Rcpp::export]]
CharacterVector path_(List paths, const char* ext) {
  R_xlen_t max_row = 0;
  R_xlen_t max_col = Rf_xlength(paths);
  char buf[PATH_MAX];
  char* b = buf;
  for (R_xlen_t c = 0; c < max_col; ++c) {
    R_xlen_t len = Rf_xlength(VECTOR_ELT(paths, c));
    if (len == 0) {
      return CharacterVector();
    }
    if (len > max_row) {
      max_row = len;
    }
  }
  CharacterVector out(max_row);
  for (R_xlen_t r = 0; r < max_row; ++r) {
    bool has_na = false;
    b = buf;
    for (R_xlen_t c = 0; c < max_col; ++c) {
      R_xlen_t k = Rf_xlength(VECTOR_ELT(paths, c));
      if (k > 0) {
        SEXP str = STRING_ELT(VECTOR_ELT(paths, c), r % k);
        if (str == NA_STRING) {
          has_na = true;
          break;
        }
        int str_len = LENGTH(str);
        int new_len = b - buf + str_len;
        if (new_len > PATH_MAX) {
          std::stringstream err;
          err << "Total path length must be less than PATH_MAX: " << PATH_MAX;
          throw Rcpp::exception(err.str().c_str(), false);
        }

        const char* s = CHAR(str);
        strncpy(b, s, str_len);
        b += str_len;

        bool trailing_slash =
            (b > buf) && (*(b - 1) == '/' || *(b - 1) == '\\');
        if (!(trailing_slash || c == (max_col - 1))) {
          *b++ = '/';
        }
      }
    }
    if (has_na) {
      out[r] = NA_STRING;
    } else {
      if (strlen(ext) > 0) {
        *b++ = '.';
        strcpy(b, ext);
        b += strlen(ext) + 1;
      }
      *b = '\0';
      out[r] = Rf_mkCharCE(buf, CE_UTF8);
    }
  }
  return out;
}

// Sets the destination with the source and translates slashes if needed
void set_path(
    char* destination,
    const char* source,
    size_t start = 0,
    size_t size = PATH_MAX) {
  size_t i = 0;
  for (; (i + start) < size && source[i] != '\0'; ++i) {
    if (source[i] == '\\') {
      destination[start + i] = '/';
    } else {
      destination[start + i] = source[i];
    }
  }
  destination[start + i] = '\0';
}

std::string expand_windows(const char* p) {
  size_t np = strlen(p);
  if (np == 0) {
    return "";
  }
  if (p[0] != '~') {
    return p;
  }

  size_t i = 0;
  for (; i < np; ++i) {
    if (p[i] == '/' || p[i] == '\\') {
      break;
    }
  }

  const char* env;
  char home[PATH_MAX] = {'\0'};
  size_t n;

  if ((env = getenv("R_FS_HOME"))) {
    set_path(home, env);
  } else if ((env = getenv("USERPROFILE"))) {
    set_path(home, env);
  } else {
    env = getenv("HOMEDRIVE");
    if (env) {
      set_path(home, env);
    }
    env = getenv("HOMEPATH");
    if (!env) {
      return p;
    }
    set_path(home, env, strlen(home), PATH_MAX);
  }

  // # ~user case
  if (i != 1) {
    char* home_str = strdup(home);
    if (home_str == NULL) {
      Rf_error("Allocation Failed");
    }
    strncpy(home, dirname(home_str), PATH_MAX - 1);
    free(home_str);

    // only copy enough characters to i
    n = strlen(home);
    strncat(home, p, i);
    home[n] = '/';
  }
  if (np > i) {
    n = strlen(home);
    strncat(home, p + i, PATH_MAX - n);
    home[n] = '/';
  }

  return home;
}

// [[Rcpp::export]]
CharacterVector expand_(CharacterVector path, bool windows) {
  CharacterVector out = CharacterVector(path.size());

  for (R_xlen_t i = 0; i < Rf_xlength(out); ++i) {
    if (STRING_ELT(path, i) == R_NaString) {
      SET_STRING_ELT(out, i, R_NaString);
    } else {
      const char* p = CHAR(STRING_ELT(path, i));
      if (windows) {
        std::string res = expand_windows(p);
        SET_STRING_ELT(out, i, Rf_mkCharCE(res.c_str(), CE_UTF8));
      } else {
        SET_STRING_ELT(out, i, Rf_mkCharCE(R_ExpandFileName(p), CE_UTF8));
      }
    }
  }
  return out;
}

// [[Rcpp::export]]
CharacterVector tidy_(CharacterVector path) {
  CharacterVector out = CharacterVector(path.size());

  for (R_xlen_t i = 0; i < Rf_xlength(out); ++i) {
    if (STRING_ELT(path, i) == R_NaString) {
      SET_STRING_ELT(out, i, R_NaString);
    } else {
      std::string p = path_tidy_(CHAR(STRING_ELT(path, i)));
      SET_STRING_ELT(out, i, Rf_mkCharCE(p.c_str(), CE_UTF8));
    }
  }
  return out;
}
