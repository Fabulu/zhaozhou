export const FORM_CPP_BACKEND_VERSION = 1;

export interface CppGeneratedFile {
  /** POSIX-relative path below the caller's output root. */
  path: string;
  /** UTF-8 text with LF line endings and one trailing LF. */
  content: string;
}

export interface CppOutput {
  files: CppGeneratedFile[];
  manifestCrc32c: number;
}
