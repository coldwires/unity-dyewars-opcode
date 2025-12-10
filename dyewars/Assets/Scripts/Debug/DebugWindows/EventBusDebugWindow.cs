#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Reflection;
using UnityEditor;
using UnityEngine;

public class EventBusDebugWindow : EditorWindow
{
    private Vector2 scrollPos;
    private Dictionary<Type, bool> foldouts = new();
    private Dictionary<Type, bool> listenerFoldouts = new();
    private Dictionary<Type, bool> publisherFoldouts = new();
    private bool allExpanded = false;

    [MenuItem("Window/EventBus Debug")]
    public static void ShowWindow()
    {
        var window = GetWindow<EventBusDebugWindow>("EventBus Debug");
        window.minSize = new Vector2(350, 250);
    }

    void OnEnable() => EditorApplication.playModeStateChanged += OnPlayModeChanged;
    void OnDisable() => EditorApplication.playModeStateChanged -= OnPlayModeChanged;

    void OnPlayModeChanged(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode || state == PlayModeStateChange.ExitingPlayMode)
        {
            foldouts.Clear();
            listenerFoldouts.Clear();
            publisherFoldouts.Clear();
        }
        Repaint();
    }

    void OnInspectorUpdate() { if (Application.isPlaying) Repaint(); }

    void OnGUI()
    {
        DrawToolbar();

        if (!Application.isPlaying)
        {
            EditorGUILayout.HelpBox("Enter Play Mode to see EventBus activity.", MessageType.Info);
            return;
        }

        scrollPos = EditorGUILayout.BeginScrollView(scrollPos);

        var eventTypes = EventBus.GetRegisteredTypes();

        if (eventTypes.Count == 0)
            EditorGUILayout.HelpBox("No events registered yet.", MessageType.Info);
        else
            foreach (var type in eventTypes)
            {
                DrawEventSection(type);
                EditorGUILayout.Space(5);
            }

        EditorGUILayout.EndScrollView();
    }

    void DrawToolbar()
    {
        EditorGUILayout.BeginHorizontal(EditorStyles.toolbar);

        if (GUILayout.Button("Refresh", EditorStyles.toolbarButton, GUILayout.Width(60)))
            Repaint();

        if (Application.isPlaying)
        {
            if (GUILayout.Button("Clear Stats", EditorStyles.toolbarButton, GUILayout.Width(80)))
                EventBus.ClearAllStats();

            var collapseLabel = allExpanded ? "Collapse All" : "Expand All";
            if (GUILayout.Button(collapseLabel, EditorStyles.toolbarButton, GUILayout.Width(80)))
            {
                allExpanded = !allExpanded;
                foreach (var key in new List<Type>(foldouts.Keys))
                {
                    foldouts[key] = allExpanded;
                    listenerFoldouts[key] = allExpanded;
                    publisherFoldouts[key] = allExpanded;
                }
            }
        }

        GUILayout.FlexibleSpace();

        var statusStyle = new GUIStyle(EditorStyles.miniLabel);
        statusStyle.normal.textColor = Application.isPlaying ? Color.green : Color.gray;
        GUILayout.Label(Application.isPlaying ? "● Playing" : "○ Stopped", statusStyle);

        EditorGUILayout.EndHorizontal();
    }

    void DrawEventSection(Type eventType)
    {
        if (!foldouts.ContainsKey(eventType)) foldouts[eventType] = true;
        if (!listenerFoldouts.ContainsKey(eventType)) listenerFoldouts[eventType] = false;
        if (!publisherFoldouts.ContainsKey(eventType)) publisherFoldouts[eventType] = false;

        var listeners = EventBus.GetListenerInfosForType(eventType);
        var publishers = EventBus.GetPublishersForType(eventType);
        var publishCount = EventBus.GetPublishCountForType(eventType);
        var lastData = EventBus.GetLastPublishedForType(eventType);

        EditorGUILayout.BeginVertical(EditorStyles.helpBox);

        // Header
        EditorGUILayout.BeginHorizontal();
        foldouts[eventType] = EditorGUILayout.Foldout(foldouts[eventType], eventType.Name, true, EditorStyles.foldoutHeader);
        GUILayout.FlexibleSpace();
        DrawBadge($"{listeners.Count} listeners", Color.cyan);
        DrawBadge($"{publishCount} publishes", Color.yellow);
        EditorGUILayout.EndHorizontal();

        // Summary - always visible
        if (publishers.Count > 0 && listeners.Count > 0)
        {
            EditorGUILayout.Space(3);
            EditorGUILayout.LabelField("Flow:", EditorStyles.boldLabel);

            var summaryStyle = new GUIStyle(EditorStyles.label);
            summaryStyle.wordWrap = true;
            summaryStyle.richText = true;

            foreach (var pub in publishers)
            {
                foreach (var lis in listeners)
                {
                    var summary = $"<color=#88ff88>{pub.gameObjectName}.{pub.componentType}.{pub.methodName}</color> → <color=#88ccff>{lis.gameObjectName}.{lis.componentType}.{lis.methodName}</color>";
                    if (pub.count > 1)
                        summary += $" <color=#ffcc88>(x{pub.count})</color>";
                    EditorGUILayout.LabelField(summary, summaryStyle);
                }
            }
        }

        if (foldouts[eventType])
        {
            EditorGUILayout.Space(5);
            EditorGUI.indentLevel++;

            // Last Data
            if (lastData != null)
            {
                EditorGUILayout.LabelField("Last Data:", EditorStyles.boldLabel);
                EditorGUI.indentLevel++;
                foreach (var field in lastData.GetType().GetFields(BindingFlags.Public | BindingFlags.Instance))
                    EditorGUILayout.LabelField(field.Name, field.GetValue(lastData)?.ToString() ?? "null");
                EditorGUI.indentLevel--;
                EditorGUILayout.Space(3);
            }

            // Listeners
            listenerFoldouts[eventType] = EditorGUILayout.Foldout(listenerFoldouts[eventType], $"Listeners ({listeners.Count})", true);
            if (listenerFoldouts[eventType])
            {
                EditorGUI.indentLevel++;
                if (listeners.Count == 0)
                    EditorGUILayout.LabelField("(none)", EditorStyles.miniLabel);
                else
                    foreach (var info in listeners)
                    {
                        EditorGUILayout.BeginVertical(EditorStyles.helpBox);
                        EditorGUILayout.LabelField("GameObject", info.gameObjectName);
                        EditorGUILayout.LabelField("Component", info.componentType);
                        EditorGUILayout.LabelField("Method", info.methodName);
                        EditorGUILayout.EndVertical();
                    }
                EditorGUI.indentLevel--;
            }

            EditorGUILayout.Space(3);

            // Publishers
            publisherFoldouts[eventType] = EditorGUILayout.Foldout(publisherFoldouts[eventType], $"Recent Publishers ({publishers.Count})", true);
            if (publisherFoldouts[eventType])
            {
                EditorGUI.indentLevel++;
                if (publishers.Count == 0)
                    EditorGUILayout.LabelField("(none yet)", EditorStyles.miniLabel);
                else
                    foreach (var pub in publishers)
                    {
                        EditorGUILayout.BeginVertical(EditorStyles.helpBox);
                        EditorGUILayout.LabelField("Time", pub.timestamp);
                        EditorGUILayout.LabelField("GameObject", pub.gameObjectName);
                        EditorGUILayout.LabelField("Component", pub.componentType);
                        EditorGUILayout.LabelField("Method", pub.methodName);
                        if (pub.count > 1)
                            EditorGUILayout.LabelField("Count", $"x{pub.count}");
                        EditorGUILayout.EndVertical();
                    }
                EditorGUI.indentLevel--;
            }

            EditorGUI.indentLevel--;
        }

        EditorGUILayout.EndVertical();
    }

    void DrawBadge(string text, Color color)
    {
        var style = new GUIStyle(EditorStyles.miniLabel);
        style.normal.textColor = color;
        GUILayout.Label($"[{text}]", style);
    }
}
#endif