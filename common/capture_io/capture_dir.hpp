#pragma once

// Folder of one capture and the names of its images. Nothing here deletes or
// overwrites a file: an existing capture is refused, never cleared.
// Spec: .ai/specs/persistencia-captura/spec.md (R3-R5)

#include <string>

// Create <dir>/left and <dir>/right. Refuses, without creating anything, when
// <dir> exists and is not an empty folder.
bool prepareCaptureDir(const std::string& dir, std::string& error);

// Pose ids are [A-Za-z0-9-]+: '_' is reserved to separate the id from the
// index in "<pose_id>_<n>.png"
bool isValidPoseId(const std::string& pose_id);

// "left/<pose_id>_<n>.png", relative to the capture folder, as written in
// detections.json. 'side' is "left" or "right"; 'n' is >= 0.
bool relativeImagePath(const std::string& side, const std::string& pose_id, int n,
                       std::string& path, std::string& error);
