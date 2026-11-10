# AI assistant: providers, protocol and data flow

The assistant (`zionos-assistant`) is optional. Without it, without keys or
without a network connection the rest of ZionOS works normally; removing the
package removes nothing else (checked by the VM tests).

## Sources consulted (2026-11-10)

No endpoint, field or model name was taken from memory.

| Provider | Source | Used for |
|---|---|---|
| Anthropic | Official Claude API reference (Messages API, streaming, errors, API versioning, model overview) | `POST /v1/messages`, headers, SSE events, error types, model IDs |
| OpenAI | Official OpenAPI specification `openai/openai-openapi` (`openapi.yaml`, spec version 2.3.0) and `developers.openai.com/api/docs` (models, streaming guide) | `POST /v1/responses`, request/usage schemas, streaming events, error object, model IDs |

`platform.openai.com/docs` refuses automated fetches (HTTP 403), so the
machine-readable specification published by OpenAI was used instead.

## Anthropic adapter (`core/ai/anthropicprovider.cpp`)

* `POST https://api.anthropic.com/v1/messages`
* Headers: `x-api-key`, `anthropic-version: 2026-10-21`, `content-type: application/json`
* Body: `model`, `max_tokens`, `stream: true`, `system`, `messages[{role, content}]`.
  No `temperature`/`top_p`/`top_k` (rejected by this API version).
* Streaming events handled: `message_start` (usage, model),
  `content_block_delta` with `text_delta` (text), `message_delta`
  (`stop_reason`, usage), `message_stop`, `error`. Thinking and other block
  types are ignored; only text is stored and replayed.
* `stop_reason`: `end_turn` → complete, `max_tokens` → truncated notice,
  `refusal` → "provider declined" notice.
* **Refusal fallback:** for the models that support it
  (`claude-opus-5-5`, `claude-sonnet-5-5`, `claude-opus-5`, `claude-fable-5-1`,
  `claude-fable-5-2`) the request includes `fallbacks: "default"` with
  `anthropic-beta: server-side-fallback-2026-07-01`, as Anthropic recommends,
  so a safety-classifier decline can be answered by Anthropic's fallback
  model in the same request. A reply served by another model is labelled in
  the UI and billed at that model's rates. It can be switched off in
  *Settings → Assistant → Privacy and limits*.
* Built-in model list: `claude-opus-5-5` (default), `claude-sonnet-5-5`,
  `claude-haiku-5-5`, `claude-fable-5-2`; "Refresh list" queries `GET /v1/models`.
  Note: Anthropic documents that `claude-fable-5-2` requires 30-day data
  retention on the account.

## OpenAI adapter (`core/ai/openaiprovider.cpp`)

* `POST https://api.openai.com/v1/responses` (Responses API)
* Header: `Authorization: Bearer <key>`
* Body: `model`, `input[{type: "message", role, content}]`, `instructions`,
  `stream: true`, `max_output_tokens`, **`store: false`**. The server default
  for `store` is `true` (responses retained for at least 30 days); ZionOS
  always sends an explicit value and defaults it to `false`. The user can
  enable it in the settings.
* Streaming events handled: `response.output_text.delta` (`delta`),
  `response.refusal.delta`, `response.completed` / `response.incomplete`
  (`response.usage.{input_tokens,output_tokens,total_tokens}`,
  `incomplete_details.reason`), `response.failed`, `error`.
* Built-in model list (flagship text models on the models page):
  `gpt-6-astra` (default), `gpt-6.1-sol`, `gpt-6.1-terra`, `gpt-6-luna`;
  "Refresh list" queries `GET /v1/models` and hides image/audio/realtime/
  embedding models.

## Behaviour common to both

| Requirement | Implementation |
|---|---|
| Common interface | `ChatProvider` / `ChatStream` (`core/ai/provider.h`); adapters only translate formats |
| Streaming | Server-sent events parsed incrementally (`SseParser`), UI updates coalesced |
| Cancellation | *Stop* aborts the HTTP request; the partial reply is kept and marked incomplete |
| Timeouts | Idle timeout (default 120 s, configurable): no bytes for that long aborts the request |
| Rate limits / errors | HTTP status + provider error type mapped to a specific, translated message; `Retry-After` is shown when present |
| No automatic retries | One `send` = one HTTP request, always. Redirects are not followed |
| Usage | Token counts shown exactly as returned; no cost is computed or estimated |
| History | JSON files in `~/.local/share/zionos-assistant/conversations` (0600); per-conversation delete, delete-all, and a switch to disable saving |
| Markdown | Rendered with raw HTML disabled; no images or other resources are loaded; links need confirmation and must be http(s) |
| Commands | Shown in a card with a warning and a *Copy* button; never executed. Copied text is stripped of bidi-control and invisible characters |
| Attachments | Only what the user explicitly attaches (a text file ≤ 256 KiB, or the clipboard/selection text). Sending with attachments always opens a review dialog showing the exact content |
| Transmission notice | Permanent line above the input naming the provider and host; *Review what will be sent…* shows the complete request text at any time |
| System data | The only system fact sent automatically is the OS name string in the system prompt (e.g. "ZionOS 0.1 (Aleph)"), visible in the review dialog. No logs, files or hardware details are collected |
| Prompt injection | Attached content is wrapped in `<attached-content>` delimiters and the system prompt instructs the model to treat it as data. The application gives the model no tools, so injected text cannot trigger any action |
| Privilege | Refuses to start as root |

## Verification status

* Mocked tests (request shapes, streaming, every error class, cancellation,
  timeout, no-retry, redirect refusal, redaction): **verified** — see
  `tst_providers`, `tst_chat`.
* Real API calls: **not verified**. No API keys were available to the
  development session (by design keys are never pasted into it). The optional
  test `tst_live_api` exists for a key holder to run; until then the adapters
  are verified against the published schemas only.
