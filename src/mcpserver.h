#ifndef MCPSERVER_H
#define MCPSERVER_H

// MCP (Model Context Protocol) server for Qtraktor.
// Speaks JSON-RPC 2.0 over stdin/stdout per MCP spec 2025-11-25.
// Exposes list, info, extract, cat, verify as typed MCP tools.
// Runs as a synchronous blocking loop — no Qt event loop needed.

int cmdMcp();

#endif // MCPSERVER_H
