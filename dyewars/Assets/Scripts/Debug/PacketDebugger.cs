// PacketDebugger.cs
// Static class for tracking sent and received packets.
// Used by PacketDebugWindow to display packet activity.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;
using DyeWars.Network.Protocol;

namespace DyeWars.Network.Debugging
{
    public static class PacketDebugger
    {
        public const int MaxHistory = 100;

        [Serializable]
        public class PacketInfo
        {
            public string timestamp;
            public byte opcode;
            public string opcodeName;
            public string category;
            public bool isSent; // true = sent, false = received
            public int size;
            public byte[] bytes;
            public string callerMethod;
            public string callerClass;
            public int count;

            public string Key => $"{opcode:X2}_{isSent}_{callerClass}_{callerMethod}";
        }

        private static List<PacketInfo> history = new();
        private static Dictionary<byte, int> sentCounts = new();
        private static Dictionary<byte, int> receivedCounts = new();
        private static Dictionary<byte, PacketInfo> lastSent = new();
        private static Dictionary<byte, PacketInfo> lastReceived = new();

        public static int TotalSent { get; private set; }
        public static int TotalReceived { get; private set; }
        public static int TotalBytesSent { get; private set; }
        public static int TotalBytesReceived { get; private set; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        private static void Init()
        {
            ClearAll();
        }

        public static void TrackSent(byte[] packet)
        {
            if (packet == null || packet.Length < 1) return;

            // Get caller info from stack trace
            var stack = new StackTrace(2, false);
            var frame = stack.GetFrame(0);
            var method = frame?.GetMethod();
            var callerMethod = method?.Name ?? "unknown";
            var callerClass = method?.DeclaringType?.Name ?? "unknown";

            byte opcode = packet[0];

            var info = new PacketInfo
            {
                timestamp = DateTime.Now.ToString("HH:mm:ss.fff"),
                opcode = opcode,
                opcodeName = OpcodeUtil.GetName(opcode),
                category = OpcodeUtil.GetCategory(opcode),
                isSent = true,
                size = packet.Length,
                bytes = (byte[])packet.Clone(),
                callerMethod = callerMethod,
                callerClass = callerClass,
                count = 1
            };

            // Update counts
            TotalSent++;
            TotalBytesSent += packet.Length;
            sentCounts[opcode] = sentCounts.GetValueOrDefault(opcode) + 1;
            lastSent[opcode] = info;

            // Add to history
            AddToHistory(info);
        }

        public static void TrackReceived(byte[] packet)
        {
            if (packet == null || packet.Length < 1) return;

            byte opcode = packet[0];

            var info = new PacketInfo
            {
                timestamp = DateTime.Now.ToString("HH:mm:ss.fff"),
                opcode = opcode,
                opcodeName = OpcodeUtil.GetName(opcode),
                category = OpcodeUtil.GetCategory(opcode),
                isSent = false,
                size = packet.Length,
                bytes = (byte[])packet.Clone(),
                callerMethod = "ProcessPacket",
                callerClass = "PacketHandler",
                count = 1
            };

            // Update counts
            TotalReceived++;
            TotalBytesReceived += packet.Length;
            receivedCounts[opcode] = receivedCounts.GetValueOrDefault(opcode) + 1;
            lastReceived[opcode] = info;

            // Add to history
            AddToHistory(info);
        }

        private static void AddToHistory(PacketInfo info)
        {
            // Check if we can merge with an existing entry (same opcode + direction in last few)
            for (int i = history.Count - 1; i >= 0 && i >= history.Count - 5; i--)
            {
                var existing = history[i];
                if (existing.opcode == info.opcode && existing.isSent == info.isSent)
                {
                    existing.count++;
                    existing.timestamp = info.timestamp;
                    existing.bytes = info.bytes;
                    existing.size = info.size;
                    return;
                }
            }

            history.Add(info);
            if (history.Count > MaxHistory)
                history.RemoveAt(0);
        }

        // Public API for debug window
        public static List<PacketInfo> GetHistory() => new(history);
        public static int GetSentCount(byte opcode) => sentCounts.GetValueOrDefault(opcode);
        public static int GetReceivedCount(byte opcode) => receivedCounts.GetValueOrDefault(opcode);
        public static PacketInfo GetLastSent(byte opcode) => lastSent.GetValueOrDefault(opcode);
        public static PacketInfo GetLastReceived(byte opcode) => lastReceived.GetValueOrDefault(opcode);

        public static List<byte> GetSentOpcodes()
        {
            var list = new List<byte>(sentCounts.Keys);
            list.Sort();
            return list;
        }

        public static List<byte> GetReceivedOpcodes()
        {
            var list = new List<byte>(receivedCounts.Keys);
            list.Sort();
            return list;
        }

        public static void ClearStats()
        {
            TotalSent = 0;
            TotalReceived = 0;
            TotalBytesSent = 0;
            TotalBytesReceived = 0;
            sentCounts.Clear();
            receivedCounts.Clear();
            lastSent.Clear();
            lastReceived.Clear();
        }

        public static void ClearHistory()
        {
            history.Clear();
        }

        public static void ClearAll()
        {
            ClearStats();
            ClearHistory();
        }

        // Helper to format bytes as hex string
        public static string FormatBytes(byte[] bytes, int maxBytes = 32)
        {
            if (bytes == null || bytes.Length == 0) return "(empty)";

            var sb = new System.Text.StringBuilder();
            int count = Math.Min(bytes.Length, maxBytes);

            for (int i = 0; i < count; i++)
            {
                if (i > 0) sb.Append(' ');
                sb.Append(bytes[i].ToString("X2"));
            }

            if (bytes.Length > maxBytes)
                sb.Append($" ... (+{bytes.Length - maxBytes} more)");

            return sb.ToString();
        }
    }
}
