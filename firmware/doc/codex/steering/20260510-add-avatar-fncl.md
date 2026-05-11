# Add Avatar Expression Function Calling

Date: 2026-05-10

## Goal

Realtime AI conversation builds should allow the AI to change Stack-chan's avatar expression in real time according to the conversation emotion.

The actual expression change is handled by `Avatar::setExpression()`.

## Scope

This feature is limited to Realtime API builds, such as:

- `env:m5stack-core2-realtime`
- `env:m5stack-cores3-realtime`
- `env:m5stack-atoms3r-realtime`

Non-Realtime builds should not expose this function to the model.

## Main Files

- `src/llm/ChatGPT/FunctionCall.cpp`
- `src/llm/ChatGPT/FunctionCall.h`
- `doc/fw_design.md`

## Proposed Function

Add a Realtime-only function calling tool named `set_avatar_expression`.

Example schema:

```json
{
  "name": "set_avatar_expression",
  "description": "Change Stack-chan's facial expression to match the current emotion.",
  "parameters": {
    "type": "object",
    "properties": {
      "expression": {
        "type": "string",
        "description": "Facial expression to set.",
        "enum": ["neutral", "happy", "angry", "sad", "doubt", "sleepy"]
      }
    },
    "required": ["expression"]
  }
}
```

## Implementation Notes

- Add the tool definition to `json_Functions` only when `REALTIME_API` is defined.
- Add the execution branch in `FunctionCall::exec_calledFunc()` only when `REALTIME_API` is defined.
- Add the corresponding method declaration in `FunctionCall.h` only when `REALTIME_API` is defined.
- Convert the `expression` string into `m5avatar::Expression`.
- Call `avatar.setExpression()` directly from `FunctionCall.cpp`.
- Return a short result string indicating success or failure.

## Expression Mapping

- `neutral` -> `Expression::Neutral`
- `happy` -> `Expression::Happy`
- `angry` -> `Expression::Angry`
- `sad` -> `Expression::Sad`
- `doubt` -> `Expression::Doubt`
- `sleepy` -> `Expression::Sleepy`

## Design Considerations

`json_Functions` is also used by non-Realtime paths such as normal ChatGPT and Gemini Live. Therefore, adding the tool unconditionally would expose it outside the intended Realtime API builds.

Use `#if defined(REALTIME_API)` around both the schema and the execution code to keep the feature scoped.

Calling `avatar.setExpression()` directly is acceptable for the first implementation because:

- `FunctionCall.cpp` already includes `Avatar.h`.
- `FunctionCall.cpp` already uses the global `avatar`.
- Existing code already changes expressions from function-related flows.
- The expression change itself is lightweight.

## Verification Plan

- Build at least `m5stack-core2-realtime`.
- If time allows, also check `m5stack-cores3-realtime` and `m5stack-atoms3r-realtime`.
- Confirm a non-Realtime build does not expose or compile the Realtime-only function path.

