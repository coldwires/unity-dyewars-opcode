#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using DyeWars.Network.Debugging;
using DyeWars.Network.Protocol;

public class PacketDebugWindow : EditorWindow
{
    private Vector2 historyScrollPos;
    private Vector2 statsScrollPos;
    private int selectedTab = 0;
    private readonly string[] tabNames = { "History", "Sent Stats", "Received Stats" };

    private Dictionary<byte, bool> foldouts = new();
    private bool showSent = true;
    private bool showReceived = true;
    private string filterText = "";
    private bool autoScroll = true;
    private bool showBytes = false;

    [MenuItem("Window/Packet Debug")]
    public static void ShowWindow()
    {
        var window = GetWindow<PacketDebugWindow>("Packet Debug");
        window.minSize = new Vector2(400, 300);
    }

    void OnEnable() => EditorApplication.playModeStateChanged += OnPlayModeChanged;
    void OnDisable() => EditorApplication.playModeStateChanged -= OnPlayModeChanged;

    void OnPlayModeChanged(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode || state == PlayModeStateChange.ExitingPlayMode)
            foldouts.Clear();
        Repaint();
    }

    void OnInspectorUpdate() { if (Application.isPlaying) Repaint(); }

    void OnGUI()
    {
        DrawToolbar();

        if (!Application.isPlaying)
        {
            EditorGUILayout.HelpBox("Enter Play Mode to see packet activity.", MessageType.Info);
            return;
        }

        DrawSummary();

        selectedTab = GUILayout.Toolbar(selectedTab, tabNames);

        EditorGUILayout.Space(5);

        switch (selectedTab)
        {
            case 0: DrawHistory(); break;
            case 1: DrawSentStats(); break;
            case 2: DrawReceivedStats(); break;
        }
    }

    void DrawToolbar()
    {
        EditorGUILayout.BeginHorizontal(EditorStyles.toolbar);

        if (GUILayout.Button("Refresh", EditorStyles.toolbarButton, GUILayout.Width(60)))
            Repaint();

        if (Application.isPlaying)
        {
            if (GUILayout.Button("Clear Stats", EditorStyles.toolbarButton, GUILayout.Width(80)))
                PacketDebugger.ClearStats();

            if (GUILayout.Button("Clear History", EditorStyles.toolbarButton, GUILayout.Width(90)))
                PacketDebugger.ClearHistory();

            if (GUILayout.Button("Clear All", EditorStyles.toolbarButton, GUILayout.Width(70)))
                PacketDebugger.ClearAll();
        }

        GUILayout.FlexibleSpace();

        var statusStyle = new GUIStyle(EditorStyles.miniLabel);
        statusStyle.normal.textColor = Application.isPlaying ? Color.green : Color.gray;
        GUILayout.Label(Application.isPlaying ? "● Playing" : "○ Stopped", statusStyle);

        EditorGUILayout.EndHorizontal();
    }

    void DrawSummary()
    {
        EditorGUILayout.BeginHorizontal(EditorStyles.helpBox);

        var sentStyle = new GUIStyle(EditorStyles.boldLabel) { normal = { textColor = new Color(0.5f, 1f, 0.5f) } };
        var recvStyle = new GUIStyle(EditorStyles.boldLabel) { normal = { textColor = new Color(0.5f, 0.8f, 1f) } };

        GUILayout.Label($"Sent: {PacketDebugger.TotalSent} ({FormatBytes(PacketDebugger.TotalBytesSent)})", sentStyle);
        GUILayout.FlexibleSpace();
        GUILayout.Label($"Received: {PacketDebugger.TotalReceived} ({FormatBytes(PacketDebugger.TotalBytesReceived)})", recvStyle);

        EditorGUILayout.EndHorizontal();
    }

    void DrawHistory()
    {
        // Filters
        EditorGUILayout.BeginHorizontal();
        showSent = GUILayout.Toggle(showSent, "Sent", EditorStyles.toolbarButton, GUILayout.Width(60));
        showReceived = GUILayout.Toggle(showReceived, "Received", EditorStyles.toolbarButton, GUILayout.Width(70));
        GUILayout.Space(10);
        showBytes = GUILayout.Toggle(showBytes, "Show Bytes", EditorStyles.toolbarButton, GUILayout.Width(80));
        autoScroll = GUILayout.Toggle(autoScroll, "Auto-scroll", EditorStyles.toolbarButton, GUILayout.Width(80));
        GUILayout.FlexibleSpace();
        filterText = EditorGUILayout.TextField(filterText, EditorStyles.toolbarSearchField, GUILayout.Width(150));
        EditorGUILayout.EndHorizontal();

        EditorGUILayout.Space(3);

        var history = PacketDebugger.GetHistory();

        historyScrollPos = EditorGUILayout.BeginScrollView(historyScrollPos);

        if (history.Count == 0)
        {
            EditorGUILayout.HelpBox("No packets recorded yet.", MessageType.Info);
        }
        else
        {
            // Draw in reverse order (newest first)
            for (int i = history.Count - 1; i >= 0; i--)
            {
                var info = history[i];

                // Filter by direction
                if (info.isSent && !showSent) continue;
                if (!info.isSent && !showReceived) continue;

                // Filter by text
                if (!string.IsNullOrEmpty(filterText))
                {
                    if (!info.opcodeName.ToLower().Contains(filterText.ToLower()) &&
                        !info.category.ToLower().Contains(filterText.ToLower()) &&
                        !info.opcode.ToString("X2").Contains(filterText.ToUpper()))
                        continue;
                }

                DrawPacketEntry(info);
            }
        }

        EditorGUILayout.EndScrollView();

        if (autoScroll && history.Count > 0)
            historyScrollPos = Vector2.zero; // Scroll to top (newest)
    }

    void DrawPacketEntry(PacketDebugger.PacketInfo info)
    {
        var bgColor = info.isSent ? new Color(0.2f, 0.35f, 0.2f) : new Color(0.2f, 0.3f, 0.4f);
        var oldBg = GUI.backgroundColor;
        GUI.backgroundColor = bgColor;

        EditorGUILayout.BeginVertical(EditorStyles.helpBox);
        GUI.backgroundColor = oldBg;

        // Header line
        EditorGUILayout.BeginHorizontal();

        var dirStyle = new GUIStyle(EditorStyles.miniLabel);
        dirStyle.normal.textColor = info.isSent ? new Color(0.5f, 1f, 0.5f) : new Color(0.5f, 0.8f, 1f);
        GUILayout.Label(info.isSent ? "→ SENT" : "← RECV", dirStyle, GUILayout.Width(50));

        GUILayout.Label($"[0x{info.opcode:X2}]", EditorStyles.miniLabel, GUILayout.Width(45));
        GUILayout.Label(info.opcodeName, EditorStyles.boldLabel);
        GUILayout.FlexibleSpace();

        var countStyle = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = Color.yellow } };
        if (info.count > 1)
            GUILayout.Label($"x{info.count}", countStyle, GUILayout.Width(40));

        GUILayout.Label($"{info.size}B", EditorStyles.miniLabel, GUILayout.Width(35));
        GUILayout.Label(info.timestamp, EditorStyles.miniLabel, GUILayout.Width(80));

        EditorGUILayout.EndHorizontal();

        // Details line
        EditorGUILayout.BeginHorizontal();
        EditorGUI.indentLevel++;

        var detailStyle = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = Color.gray } };
        GUILayout.Label($"Category: {info.category}", detailStyle);

        if (info.isSent)
            GUILayout.Label($"Caller: {info.callerClass}.{info.callerMethod}()", detailStyle);

        EditorGUI.indentLevel--;
        EditorGUILayout.EndHorizontal();

        // Bytes line (optional)
        if (showBytes && info.bytes != null)
        {
            var bytesStyle = new GUIStyle(EditorStyles.miniLabel)
            {
                normal = { textColor = new Color(0.7f, 0.7f, 0.7f) },
                wordWrap = true,
                fontStyle = FontStyle.Italic
            };
            EditorGUILayout.LabelField(PacketDebugger.FormatBytes(info.bytes), bytesStyle);
        }

        EditorGUILayout.EndVertical();
    }

    void DrawSentStats()
    {
        var opcodes = PacketDebugger.GetSentOpcodes();

        statsScrollPos = EditorGUILayout.BeginScrollView(statsScrollPos);

        if (opcodes.Count == 0)
        {
            EditorGUILayout.HelpBox("No packets sent yet.", MessageType.Info);
        }
        else
        {
            foreach (var opcode in opcodes)
            {
                DrawOpcodeStats(opcode, true);
            }
        }

        EditorGUILayout.EndScrollView();
    }

    void DrawReceivedStats()
    {
        var opcodes = PacketDebugger.GetReceivedOpcodes();

        statsScrollPos = EditorGUILayout.BeginScrollView(statsScrollPos);

        if (opcodes.Count == 0)
        {
            EditorGUILayout.HelpBox("No packets received yet.", MessageType.Info);
        }
        else
        {
            foreach (var opcode in opcodes)
            {
                DrawOpcodeStats(opcode, false);
            }
        }

        EditorGUILayout.EndScrollView();
    }

    void DrawOpcodeStats(byte opcode, bool isSent)
    {
        var count = isSent ? PacketDebugger.GetSentCount(opcode) : PacketDebugger.GetReceivedCount(opcode);
        var last = isSent ? PacketDebugger.GetLastSent(opcode) : PacketDebugger.GetLastReceived(opcode);

        if (!foldouts.ContainsKey(opcode)) foldouts[opcode] = false;

        EditorGUILayout.BeginVertical(EditorStyles.helpBox);

        // Header
        EditorGUILayout.BeginHorizontal();

        foldouts[opcode] = EditorGUILayout.Foldout(foldouts[opcode], $"[0x{opcode:X2}] {OpcodeUtil.GetName(opcode)}", true);

        GUILayout.FlexibleSpace();
        DrawBadge($"{count}x", Color.yellow);

        EditorGUILayout.EndHorizontal();

        // Details
        if (foldouts[opcode] && last != null)
        {
            EditorGUI.indentLevel++;

            EditorGUILayout.LabelField("Category", last.category);
            EditorGUILayout.LabelField("Last Size", $"{last.size} bytes");
            EditorGUILayout.LabelField("Last Time", last.timestamp);

            if (isSent)
                EditorGUILayout.LabelField("Caller", $"{last.callerClass}.{last.callerMethod}()");

            EditorGUILayout.Space(3);
            EditorGUILayout.LabelField("Last Bytes:");

            var bytesStyle = new GUIStyle(EditorStyles.textArea)
            {
                wordWrap = true,
                fontStyle = FontStyle.Italic,
                fontSize = 10
            };
            EditorGUILayout.SelectableLabel(PacketDebugger.FormatBytes(last.bytes, 64), bytesStyle, GUILayout.Height(40));

            EditorGUI.indentLevel--;
        }

        EditorGUILayout.EndVertical();
    }

    void DrawBadge(string text, Color color)
    {
        var style = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = color } };
        GUILayout.Label($"[{text}]", style);
    }

    string FormatBytes(int bytes)
    {
        if (bytes < 1024) return $"{bytes}B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024f:F1}KB";
        return $"{bytes / (1024f * 1024f):F1}MB";
    }
}
#endif
