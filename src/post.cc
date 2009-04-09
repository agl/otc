#include "otc.h"
#include "post.h"
#include "maxp.h"

bool
otc_post_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypePOST *post = new OpenTypePOST;
  file->post = post;

  if (!table.ReadU32(&post->version) ||
      !table.ReadU32(&post->italic_angle) ||
      !table.ReadU16(&post->underline) ||
      !table.ReadU16(&post->underline_thickness) ||
      !table.ReadU32(&post->is_fixed_pitch)) {
    return failure();
  }

  if (post->version == 0x00010000) {
    return true;
  } else if (post->version == 0x00030000) {
    return true;
  } else if (post->version != 0x00020000) {
    return failure();
  }

  // We have a version 2 table with a list of Pascal strings at the end

  // We don't care about the memory usage fields. We'll set all these to zero
  // when serialising
  if (!table.Skip(16))
    return failure();

  uint16_t num_glyphs;
  if (!table.ReadU16(&num_glyphs))
    return failure();

  if (!file->maxp)
    return failure();

  if (num_glyphs != file->maxp->num_glyphs)
    return failure();

  post->glyph_name_index.resize(num_glyphs);
  for (unsigned i = 0; i < num_glyphs; ++i) {
    if (!table.ReadU16(&post->glyph_name_index[i]))
      return failure();
    if (post->glyph_name_index[i] >= 32768)
      return failure();
  }

  // Now we have an array of Pascal strings. We have to check that they are all
  // valid and read them in.
  const size_t strings_offset = table.offset();
  const uint8_t *strings = data + strings_offset;
  const uint8_t *strings_end = data + length;

  for (;;) {
    if (strings == strings_end)
      break;
    const unsigned string_length = *strings;
    if (strings + 1 + string_length > strings_end)
      return failure();
    const std::string s((char *) strings + 1, string_length);
    post->names.push_back(s);
    strings += 1 + string_length;
  }
  const unsigned num_strings = post->names.size();

  // check that all the references are within bounds
  for (unsigned i = 0; i < num_glyphs; ++i) {
    unsigned offset = post->glyph_name_index[i];
    if (offset < 258)
      continue;

    offset -= 258;
    if (offset >= num_strings)
      return failure();
  }

  return true;
}

bool
otc_post_should_serialise(OpenTypeFile *file) {
  return file->post;
}

bool
otc_post_serialise(OTCStream *out, OpenTypeFile *file) {
  const OpenTypePOST *post = file->post;

  if (!out->WriteU32(post->version) ||
      !out->WriteU32(post->italic_angle) ||
      !out->WriteU16(post->underline) ||
      !out->WriteU16(post->underline_thickness) ||
      !out->WriteU32(post->is_fixed_pitch) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0)) {
    return failure();
  }


  if (post->version != 0x00020000)
    return true;

  if (!out->WriteU16(post->glyph_name_index.size()))
    return failure();

  for (unsigned i = 0; i < post->glyph_name_index.size(); ++i) {
    if (!out->WriteU16(post->glyph_name_index[i]))
      return failure();
  }

  // Now we just have to write out the strings in the correct order
  for (unsigned i = 0; i < post->names.size(); ++i) {
    const std::string& s = post->names[i];
    const uint8_t string_length = s.size();
    if (!out->Write(&string_length, 1) ||
        !out->Write(s.data(), string_length)) {
      return failure();
    }
  }

  return true;
}

void
otc_post_free(OpenTypeFile *file) {
  delete file->post;
}
