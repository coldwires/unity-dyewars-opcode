#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Reflection;
using UnityEditor;
using UnityEngine;

public class EventBusDebugWindow2 : EditorWindow
{
    private Vector2 historyScrollPos;
    private Vector2 statsScrollPos;
    private int selectedTab = 0;
    private readonly string[] tabNames = { "History", "Event Stats", "Listeners" };

    private Dictionary<Type, bool> foldouts = new();
    private bool showBytes = false;
    private string filterText = "";
    private bool autoScroll = true;

    // History tracking
    private static List<EventHistoryEntry> history = new();
    private const int MaxHistory = 100;

    [Serializable]
    public class EventHistoryEntry
    {
        public string timestamp;
        public string eventName;
        public string category;
        public string publisher;
        public string publisherComponent;
        public string publisherMethod;
        public int listenerCount;
        public List<EventBus.ListenerInfo> listeners;
        public object eventData;
        public int count;

        public string Key => $"{eventName}_{publisher}_{publisherMethod}";
    }

    [MenuItem("Window/EventBus Debug 2")]
    public static void ShowWindow()
    {
        var window = GetWindow<EventBusDebugWindow2>("EventBus Debug 2");
        window.minSize = new Vector2(400, 300);
    }

    void OnEnable()
    {
        EditorApplication.playModeStateChanged += OnPlayModeChanged;
    }

    void OnDisable()
    {
        EditorApplication.playModeStateChanged -= OnPlayModeChanged;
    }

    void OnPlayModeChanged(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode || state == PlayModeStateChange.ExitingPlayMode)
        {
            foldouts.Clear();
            history.Clear();
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

        DrawSummary();

        selectedTab = GUILayout.Toolbar(selectedTab, tabNames);

        EditorGUILayout.Space(5);

        switch (selectedTab)
        {
            case 0: DrawHistory(); break;
            case 1: DrawEventStats(); break;
            case 2: DrawListeners(); break;
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
                EventBus.ClearAllStats();

            if (GUILayout.Button("Clear History", EditorStyles.toolbarButton, GUILayout.Width(90)))
                history.Clear();

            if (GUILayout.Button("Clear All", EditorStyles.toolbarButton, GUILayout.Width(70)))
            {
                EventBus.ClearAllStats();
                history.Clear();
            }
        }

        GUILayout.FlexibleSpace();

        var statusStyle = new GUIStyle(EditorStyles.miniLabel);
        statusStyle.normal.textColor = Application.isPlaying ? Color.green : Color.gray;
        GUILayout.Label(Application.isPlaying ? "● Playing" : "○ Stopped", statusStyle);

        EditorGUILayout.EndHorizontal();
    }

    void DrawSummary()
    {
        var eventTypes = EventBus.GetRegisteredTypes();
        int totalListeners = 0;
        int totalPublishes = 0;

        foreach (var type in eventTypes)
        {
            totalListeners += EventBus.GetListenerInfosForType(type).Count;
            totalPublishes += EventBus.GetPublishCountForType(type);
        }

        EditorGUILayout.BeginHorizontal(EditorStyles.helpBox);

        var typeStyle = new GUIStyle(EditorStyles.boldLabel) { normal = { textColor = new Color(1f, 0.8f, 0.5f) } };
        var listenerStyle = new GUIStyle(EditorStyles.boldLabel) { normal = { textColor = new Color(0.5f, 1f, 0.5f) } };
        var publishStyle = new GUIStyle(EditorStyles.boldLabel) { normal = { textColor = new Color(0.5f, 0.8f, 1f) } };

        GUILayout.Label($"Events: {eventTypes.Count}", typeStyle);
        GUILayout.FlexibleSpace();
        GUILayout.Label($"Listeners: {totalListeners}", listenerStyle);
        GUILayout.FlexibleSpace();
        GUILayout.Label($"Publishes: {totalPublishes}", publishStyle);

        EditorGUILayout.EndHorizontal();
    }

    void DrawHistory()
    {
        // Build history from EventBus data
        RefreshHistory();

        // Filters
        EditorGUILayout.BeginHorizontal();
        showBytes = GUILayout.Toggle(showBytes, "Show Data", EditorStyles.toolbarButton, GUILayout.Width(80));
        autoScroll = GUILayout.Toggle(autoScroll, "Auto-scroll", EditorStyles.toolbarButton, GUILayout.Width(80));
        GUILayout.FlexibleSpace();
        filterText = EditorGUILayout.TextField(filterText, EditorStyles.toolbarSearchField, GUILayout.Width(150));
        EditorGUILayout.EndHorizontal();

        EditorGUILayout.Space(3);

        historyScrollPos = EditorGUILayout.BeginScrollView(historyScrollPos);

        if (history.Count == 0)
        {
            EditorGUILayout.HelpBox("No events published yet.", MessageType.Info);
        }
        else
        {
            // Draw in reverse order (newest first)
            for (int i = history.Count - 1; i >= 0; i--)
            {
                var entry = history[i];

                // Filter by text
                if (!string.IsNullOrEmpty(filterText))
                {
                    if (!entry.eventName.ToLower().Contains(filterText.ToLower()) &&
                        !entry.category.ToLower().Contains(filterText.ToLower()) &&
                        !entry.publisher.ToLower().Contains(filterText.ToLower()))
                        continue;
                }

                DrawHistoryEntry(entry);
            }
        }

        EditorGUILayout.EndScrollView();

        if (autoScroll && history.Count > 0)
            historyScrollPos = Vector2.zero;
    }

    void RefreshHistory()
    {
        // Rebuild history from EventBus publishers
        var eventTypes = EventBus.GetRegisteredTypes();

        foreach (var type in eventTypes)
        {
            var publishers = EventBus.GetPublishersForType(type);
            var listeners = EventBus.GetListenerInfosForType(type);
            var lastData = EventBus.GetLastPublishedForType(type);

            foreach (var pub in publishers)
            {
                var key = $"{type.Name}_{pub.gameObjectName}_{pub.methodName}";

                // Check if entry exists
                var existing = history.Find(h => h.Key == key);
                if (existing != null)
                {
                    existing.count = pub.count;
                    existing.timestamp = pub.timestamp;
                    existing.eventData = lastData;
                    existing.listenerCount = listeners.Count;
                    existing.listeners = new List<EventBus.ListenerInfo>(listeners);
                }
                else
                {
                    history.Add(new EventHistoryEntry
                    {
                        timestamp = pub.timestamp,
                        eventName = type.Name,
                        category = GetEventCategory(type.Name),
                        publisher = pub.gameObjectName,
                        publisherComponent = pub.componentType,
                        publisherMethod = pub.methodName,
                        listenerCount = listeners.Count,
                        listeners = new List<EventBus.ListenerInfo>(listeners),
                        eventData = lastData,
                        count = pub.count
                    });

                    if (history.Count > MaxHistory)
                        history.RemoveAt(0);
                }
            }
        }
    }

    void DrawHistoryEntry(EventHistoryEntry entry)
    {
        var bgColor = GetCategoryColor(entry.category);
        var oldBg = GUI.backgroundColor;
        GUI.backgroundColor = bgColor;

        EditorGUILayout.BeginVertical(EditorStyles.helpBox);
        GUI.backgroundColor = oldBg;

        // Header line
        EditorGUILayout.BeginHorizontal();

        var dirStyle = new GUIStyle(EditorStyles.miniLabel);
        dirStyle.normal.textColor = new Color(1f, 0.8f, 0.5f);
        GUILayout.Label("⚡ EVENT", dirStyle, GUILayout.Width(55));

        GUILayout.Label(entry.eventName, EditorStyles.boldLabel);
        GUILayout.FlexibleSpace();

        var countStyle = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = Color.yellow } };
        if (entry.count > 1)
            GUILayout.Label($"x{entry.count}", countStyle, GUILayout.Width(40));

        var listenerStyle = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = Color.cyan } };
        GUILayout.Label($"{entry.listenerCount} listeners", listenerStyle, GUILayout.Width(70));

        GUILayout.Label(entry.timestamp, EditorStyles.miniLabel, GUILayout.Width(80));

        EditorGUILayout.EndHorizontal();

        // Flow line - publisher -> listeners
        var flowStyle = new GUIStyle(EditorStyles.miniLabel)
        {
            richText = true,
            wordWrap = true
        };

        if (entry.listeners != null && entry.listeners.Count > 0)
        {
            foreach (var listener in entry.listeners)
            {
                var flowText = $"<color=#88ff88>{entry.publisher}.{entry.publisherComponent}.{entry.publisherMethod}()</color> → <color=#88ccff>{listener.gameObjectName}.{listener.componentType}.{listener.methodName}()</color>";
                EditorGUILayout.LabelField(flowText, flowStyle);
            }
        }
        else
        {
            var noListenerText = $"<color=#88ff88>{entry.publisher}.{entry.publisherComponent}.{entry.publisherMethod}()</color> → <color=#888888>(no listeners)</color>";
            EditorGUILayout.LabelField(noListenerText, flowStyle);
        }

        // Category line
        var detailStyle = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = Color.gray } };
        EditorGUILayout.LabelField($"Category: {entry.category}", detailStyle);

        // Data (optional)
        if (showBytes && entry.eventData != null)
        {
            var dataStyle = new GUIStyle(EditorStyles.miniLabel)
            {
                normal = { textColor = new Color(0.7f, 0.7f, 0.7f) },
                wordWrap = true,
                fontStyle = FontStyle.Italic
            };
            EditorGUILayout.LabelField(FormatEventData(entry.eventData), dataStyle);
        }

        EditorGUILayout.EndVertical();
    }

    void DrawEventStats()
    {
        var eventTypes = EventBus.GetRegisteredTypes();

        statsScrollPos = EditorGUILayout.BeginScrollView(statsScrollPos);

        if (eventTypes.Count == 0)
        {
            EditorGUILayout.HelpBox("No events registered yet.", MessageType.Info);
        }
        else
        {
            foreach (var type in eventTypes)
            {
                DrawEventTypeStats(type);
            }
        }

        EditorGUILayout.EndScrollView();
    }

    void DrawEventTypeStats(Type eventType)
    {
        var listeners = EventBus.GetListenerInfosForType(eventType);
        var publishers = EventBus.GetPublishersForType(eventType);
        var publishCount = EventBus.GetPublishCountForType(eventType);
        var lastData = EventBus.GetLastPublishedForType(eventType);

        if (!foldouts.ContainsKey(eventType)) foldouts[eventType] = false;

        var category = GetEventCategory(eventType.Name);
        var bgColor = GetCategoryColor(category);
        var oldBg = GUI.backgroundColor;
        GUI.backgroundColor = bgColor;

        EditorGUILayout.BeginVertical(EditorStyles.helpBox);
        GUI.backgroundColor = oldBg;

        // Header
        EditorGUILayout.BeginHorizontal();

        foldouts[eventType] = EditorGUILayout.Foldout(foldouts[eventType], eventType.Name, true);

        GUILayout.FlexibleSpace();
        DrawBadge($"{listeners.Count} listeners", Color.cyan);
        DrawBadge($"{publishCount}x", Color.yellow);

        EditorGUILayout.EndHorizontal();

        // Details
        if (foldouts[eventType])
        {
            EditorGUI.indentLevel++;

            EditorGUILayout.LabelField("Category", category);

            if (lastData != null)
            {
                EditorGUILayout.Space(3);
                EditorGUILayout.LabelField("Last Data:", EditorStyles.boldLabel);

                var dataStyle = new GUIStyle(EditorStyles.textArea)
                {
                    wordWrap = true,
                    fontSize = 10
                };
                EditorGUILayout.SelectableLabel(FormatEventData(lastData), dataStyle, GUILayout.Height(50));
            }

            if (publishers.Count > 0)
            {
                EditorGUILayout.Space(3);
                EditorGUILayout.LabelField("Publishers:", EditorStyles.boldLabel);
                foreach (var pub in publishers)
                {
                    EditorGUILayout.LabelField($"  {pub.gameObjectName}.{pub.methodName}() x{pub.count}");
                }
            }

            EditorGUI.indentLevel--;
        }

        EditorGUILayout.EndVertical();
    }

    void DrawListeners()
    {
        var eventTypes = EventBus.GetRegisteredTypes();

        statsScrollPos = EditorGUILayout.BeginScrollView(statsScrollPos);

        if (eventTypes.Count == 0)
        {
            EditorGUILayout.HelpBox("No events registered yet.", MessageType.Info);
        }
        else
        {
            foreach (var type in eventTypes)
            {
                var listeners = EventBus.GetListenerInfosForType(type);
                if (listeners.Count == 0) continue;

                var category = GetEventCategory(type.Name);
                var bgColor = GetCategoryColor(category);
                var oldBg = GUI.backgroundColor;
                GUI.backgroundColor = bgColor;

                EditorGUILayout.BeginVertical(EditorStyles.helpBox);
                GUI.backgroundColor = oldBg;

                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(type.Name, EditorStyles.boldLabel);
                GUILayout.FlexibleSpace();
                DrawBadge($"{listeners.Count} listeners", Color.cyan);
                EditorGUILayout.EndHorizontal();

                EditorGUI.indentLevel++;
                foreach (var listener in listeners)
                {
                    var listenerStyle = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = new Color(0.5f, 1f, 0.5f) } };
                    EditorGUILayout.LabelField($"→ {listener.gameObjectName}.{listener.componentType}.{listener.methodName}()", listenerStyle);
                }
                EditorGUI.indentLevel--;

                EditorGUILayout.EndVertical();
            }
        }

        EditorGUILayout.EndScrollView();
    }

    void DrawBadge(string text, Color color)
    {
        var style = new GUIStyle(EditorStyles.miniLabel) { normal = { textColor = color } };
        GUILayout.Label($"[{text}]", style);
    }

    string GetEventCategory(string eventName)
    {
        if (eventName.Contains("Player")) return "Player";
        if (eventName.Contains("Position") || eventName.Contains("Facing") || eventName.Contains("Move")) return "Movement";
        if (eventName.Contains("Connect") || eventName.Contains("Disconnect") || eventName.Contains("Welcome")) return "Connection";
        if (eventName.Contains("Input") || eventName.Contains("Direction")) return "Input";
        if (eventName.Contains("Combat") || eventName.Contains("Damage") || eventName.Contains("Attack")) return "Combat";
        if (eventName.Contains("Chat") || eventName.Contains("Message")) return "Chat";
        return "General";
    }

    Color GetCategoryColor(string category)
    {
        return category switch
        {
            "Player" => new Color(0.2f, 0.35f, 0.2f),
            "Movement" => new Color(0.2f, 0.3f, 0.4f),
            "Connection" => new Color(0.35f, 0.25f, 0.2f),
            "Input" => new Color(0.3f, 0.2f, 0.35f),
            "Combat" => new Color(0.4f, 0.2f, 0.2f),
            "Chat" => new Color(0.2f, 0.35f, 0.35f),
            _ => new Color(0.25f, 0.25f, 0.25f)
        };
    }

    string FormatEventData(object data)
    {
        if (data == null) return "(null)";

        var sb = new System.Text.StringBuilder();
        var fields = data.GetType().GetFields(BindingFlags.Public | BindingFlags.Instance);

        foreach (var field in fields)
        {
            if (sb.Length > 0) sb.Append(" | ");
            sb.Append($"{field.Name}: {field.GetValue(data) ?? "null"}");
        }

        return sb.Length > 0 ? sb.ToString() : data.ToString();
    }
}
#endif
