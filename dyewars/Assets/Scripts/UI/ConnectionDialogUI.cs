// ConnectionDialogUI.cs
// Unity UI version of connection dialog with crisp TextMeshPro text.
// Creates its own Canvas and UI elements at runtime - just add this component to any GameObject.

using UnityEngine;
using UnityEngine.UI;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using TMPro;
using DyeWars.Network;
using DyeWars.Core;

namespace DyeWars.UI
{
    public class ConnectionDialogUI : MonoBehaviour
    {
        [Header("Defaults")]
        [SerializeField] private string defaultHost = "127.0.0.1";
        [SerializeField] private int defaultPort = 8080;

        [Header("Prefabs")]
        [SerializeField] private TMP_InputField inputFieldPrefab; // Assign Unity's TMP InputField prefab

        [Header("Styling")]
        [SerializeField] private int fontSize = 18;
        [SerializeField] private Color backgroundColor = new Color(0.12f, 0.12f, 0.14f, 0.98f);
        [SerializeField] private Color panelBorderColor = new Color(0.3f, 0.3f, 0.35f, 1f);
        [SerializeField] private Color inputFieldColor = new Color(0.08f, 0.08f, 0.1f, 1f);
        [SerializeField] private Color inputFieldBorderColor = new Color(0.25f, 0.25f, 0.3f, 1f);
        [SerializeField] private Color buttonColor = new Color(0.2f, 0.4f, 0.7f, 1f);
        [SerializeField] private Color buttonHoverColor = new Color(0.25f, 0.5f, 0.85f, 1f);
        [SerializeField] private Color textColor = new Color(0.9f, 0.9f, 0.92f, 1f);
        [SerializeField] private Color labelColor = new Color(0.6f, 0.6f, 0.65f, 1f);

        // UI References
        private Canvas canvas;
        private GameObject panel;
        private TMP_InputField hostInput;
        private TMP_InputField portInput;
        private TextMeshProUGUI statusText;
        private Button connectButton;

        private INetworkService networkService;

        private void Start()
        {
            CreateUI();

            hostInput.text = defaultHost;
            portInput.text = defaultPort.ToString();

            // Enter key triggers connect
            hostInput.onSubmit.AddListener(_ => TryConnect());
            portInput.onSubmit.AddListener(_ => TryConnect());

            Debug.Log("ConnectionDialogUI: Created");
        }

        private void OnEnable()
        {
            EventBus.Subscribe<DisconnectedFromServerEvent>(OnDisconnected);
        }

        private void OnDisable()
        {
            EventBus.Unsubscribe<DisconnectedFromServerEvent>(OnDisconnected);
        }

        private void Update()
        {
            if (networkService == null)
            {
                networkService = ServiceLocator.Get<INetworkService>();
            }

            // Hide when connected
            if (networkService != null && networkService.IsConnected && panel != null && panel.activeSelf)
            {
                panel.SetActive(false);
                Debug.Log("ConnectionDialogUI: Connected, hiding");
            }
        }

        private void OnDisconnected(DisconnectedFromServerEvent evt)
        {
            if (panel != null)
            {
                panel.SetActive(true);
                SetStatus($"Disconnected: {evt.Reason}", Color.red);
            }
        }

        private void CreateUI()
        {
            // Ensure EventSystem exists (required for UI interaction)
            if (FindAnyObjectByType<EventSystem>() == null)
            {
                var eventSystemObj = new GameObject("EventSystem");
                eventSystemObj.AddComponent<EventSystem>();
                eventSystemObj.AddComponent<InputSystemUIInputModule>();
                Debug.Log("ConnectionDialogUI: Created EventSystem");
            }

            // Check if we're already under a Canvas
            canvas = GetComponentInParent<Canvas>();

            // If not under a Canvas, find one in the scene and parent to it
            if (canvas == null)
            {
                canvas = FindAnyObjectByType<Canvas>();
                if (canvas != null)
                {
                    transform.SetParent(canvas.transform, false);
                    Debug.Log("ConnectionDialogUI: Attached to existing Canvas");
                }
                else
                {
                    // No Canvas in scene, create one on this GameObject
                    gameObject.AddComponent<RectTransform>();
                    canvas = gameObject.AddComponent<Canvas>();
                    canvas.renderMode = RenderMode.ScreenSpaceOverlay;
                    canvas.sortingOrder = 100;
                    var scaler = gameObject.AddComponent<CanvasScaler>();
                    scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
                    scaler.referenceResolution = new Vector2(1920, 1080);
                    gameObject.AddComponent<GraphicRaycaster>();
                    Debug.Log("ConnectionDialogUI: Created new Canvas");
                }
            }

            // Ensure we have a RectTransform (needed when under a Canvas)
            var myRect = GetComponent<RectTransform>();
            if (myRect == null)
            {
                myRect = gameObject.AddComponent<RectTransform>();
            }

            // Stretch to fill parent canvas
            myRect.anchorMin = Vector2.zero;
            myRect.anchorMax = Vector2.one;
            myRect.offsetMin = Vector2.zero;
            myRect.offsetMax = Vector2.zero;

            // Create Panel (background with border effect)
            panel = CreatePanel(transform, "Panel", 380, 240);
            var panelRect = panel.GetComponent<RectTransform>();
            panelRect.anchorMin = new Vector2(0.5f, 0.5f);
            panelRect.anchorMax = new Vector2(0.5f, 0.5f);
            panelRect.anchoredPosition = Vector2.zero;

            // Add outline for border effect
            var outline = panel.AddComponent<Outline>();
            outline.effectColor = panelBorderColor;
            outline.effectDistance = new Vector2(2, 2);

            // Title
            var title = CreateText(panel.transform, "Title", "Connect to Server", 26, TextAlignmentOptions.Center,
                new Vector2(0, 85), new Vector2(340, 45));
            title.fontStyle = FontStyles.Bold;

            // Divider line
            var divider = new GameObject("Divider");
            divider.transform.SetParent(panel.transform, false);
            var dividerRect = divider.AddComponent<RectTransform>();
            dividerRect.anchoredPosition = new Vector2(0, 55);
            dividerRect.sizeDelta = new Vector2(320, 1);
            var dividerImage = divider.AddComponent<Image>();
            dividerImage.color = panelBorderColor;

            // Host Label + Input
            var hostLabel = CreateText(panel.transform, "HostLabel", "HOST", fontSize - 4, TextAlignmentOptions.Left,
                new Vector2(-140, 30), new Vector2(80, 20));
            hostLabel.color = labelColor;
            hostLabel.fontStyle = FontStyles.Bold;
            hostInput = CreateInputField(panel.transform, "HostInput", defaultHost,
                new Vector2(30, 5), new Vector2(240, 38));

            // Port Label + Input
            var portLabel = CreateText(panel.transform, "PortLabel", "PORT", fontSize - 4, TextAlignmentOptions.Left,
                new Vector2(-140, -35), new Vector2(80, 20));
            portLabel.color = labelColor;
            portLabel.fontStyle = FontStyles.Bold;
            portInput = CreateInputField(panel.transform, "PortInput", defaultPort.ToString(),
                new Vector2(-70, -60), new Vector2(100, 38));
            portInput.contentType = TMP_InputField.ContentType.IntegerNumber;

            // Status Text
            statusText = CreateText(panel.transform, "Status", "", fontSize - 2, TextAlignmentOptions.Center,
                new Vector2(0, -95), new Vector2(340, 25));

            // Connect Button
            connectButton = CreateButton(panel.transform, "ConnectButton", "CONNECT",
                new Vector2(95, -60), new Vector2(130, 38));
            connectButton.onClick.AddListener(TryConnect);
        }

        private GameObject CreatePanel(Transform parent, string name, float width, float height)
        {
            var obj = new GameObject(name);
            obj.transform.SetParent(parent, false);

            var rect = obj.AddComponent<RectTransform>();
            rect.sizeDelta = new Vector2(width, height);

            var image = obj.AddComponent<Image>();
            image.color = backgroundColor;

            return obj;
        }

        private TextMeshProUGUI CreateText(Transform parent, string name, string text, int size,
            TextAlignmentOptions alignment, Vector2 position, Vector2 sizeDelta)
        {
            var obj = new GameObject(name);
            obj.transform.SetParent(parent, false);

            var rect = obj.AddComponent<RectTransform>();
            rect.anchoredPosition = position;
            rect.sizeDelta = sizeDelta;

            var tmp = obj.AddComponent<TextMeshProUGUI>();
            tmp.text = text;
            tmp.fontSize = size;
            tmp.alignment = alignment;
            tmp.color = textColor;

            return tmp;
        }

        private TMP_InputField CreateInputField(Transform parent, string name, string placeholderText,
            Vector2 position, Vector2 sizeDelta)
        {
            // Use prefab if assigned (recommended - has working caret)
            if (inputFieldPrefab != null)
            {
                var inputField = Instantiate(inputFieldPrefab, parent);
                inputField.name = name;
                var rect = inputField.GetComponent<RectTransform>();
                rect.anchoredPosition = position;
                rect.sizeDelta = sizeDelta;

                // Apply styling
                var image = inputField.GetComponent<Image>();
                if (image != null) image.color = inputFieldColor;

                if (inputField.textComponent != null)
                {
                    inputField.textComponent.fontSize = fontSize;
                    inputField.textComponent.color = textColor;
                }

                if (inputField.placeholder is TextMeshProUGUI placeholder)
                {
                    placeholder.text = placeholderText;
                    placeholder.fontSize = fontSize;
                }

                // Caret settings
                inputField.customCaretColor = true;
                inputField.caretColor = textColor;

                return inputField;
            }

            // Fallback: create programmatically (caret may not work)
            Debug.LogWarning("ConnectionDialogUI: No inputFieldPrefab assigned. Caret may not display. Assign a TMP InputField prefab.");

            var obj = new GameObject(name);
            obj.transform.SetParent(parent, false);

            var objRect = obj.AddComponent<RectTransform>();
            objRect.anchoredPosition = position;
            objRect.sizeDelta = sizeDelta;

            obj.AddComponent<CanvasRenderer>();

            var bgImage = obj.AddComponent<Image>();
            bgImage.color = inputFieldColor;

            // Text Area
            var textArea = new GameObject("Text Area");
            textArea.transform.SetParent(obj.transform, false);
            var textAreaRect = textArea.AddComponent<RectTransform>();
            textAreaRect.anchorMin = Vector2.zero;
            textAreaRect.anchorMax = Vector2.one;
            textAreaRect.offsetMin = new Vector2(10, 5);
            textAreaRect.offsetMax = new Vector2(-10, -5);
            var rectMask = textArea.AddComponent<RectMask2D>();
            rectMask.padding = new Vector4(-8, -5, -8, -5);

            // Text
            var textObj = new GameObject("Text");
            textObj.transform.SetParent(textArea.transform, false);
            var textRect = textObj.AddComponent<RectTransform>();
            textRect.anchorMin = Vector2.zero;
            textRect.anchorMax = Vector2.one;
            textRect.offsetMin = Vector2.zero;
            textRect.offsetMax = Vector2.zero;

            var inputText = textObj.AddComponent<TextMeshProUGUI>();
            inputText.font = TMP_Settings.defaultFontAsset;
            inputText.fontSize = fontSize;
            inputText.color = textColor;
            inputText.text = "\u200B";

            // Placeholder
            var placeholderObj = new GameObject("Placeholder");
            placeholderObj.transform.SetParent(textArea.transform, false);
            var placeholderRect = placeholderObj.AddComponent<RectTransform>();
            placeholderRect.anchorMin = Vector2.zero;
            placeholderRect.anchorMax = Vector2.one;
            placeholderRect.offsetMin = Vector2.zero;
            placeholderRect.offsetMax = Vector2.zero;

            var placeholderTmp = placeholderObj.AddComponent<TextMeshProUGUI>();
            placeholderTmp.font = TMP_Settings.defaultFontAsset;
            placeholderTmp.text = placeholderText;
            placeholderTmp.fontSize = fontSize;
            placeholderTmp.color = new Color(0.5f, 0.5f, 0.5f, 0.5f);

            // Input Field
            var field = obj.AddComponent<TMP_InputField>();
            field.targetGraphic = bgImage;
            field.textViewport = textAreaRect;
            field.textComponent = inputText;
            field.placeholder = placeholderTmp;
            field.customCaretColor = true;
            field.caretColor = textColor;

            return field;
        }

        private Button CreateButton(Transform parent, string name, string text,
            Vector2 position, Vector2 sizeDelta)
        {
            var obj = new GameObject(name);
            obj.transform.SetParent(parent, false);

            var rect = obj.AddComponent<RectTransform>();
            rect.anchoredPosition = position;
            rect.sizeDelta = sizeDelta;

            var image = obj.AddComponent<Image>();
            image.color = buttonColor;

            var button = obj.AddComponent<Button>();
            button.targetGraphic = image;

            // Button colors - smooth transitions
            var colors = button.colors;
            colors.normalColor = buttonColor;
            colors.highlightedColor = buttonHoverColor;
            colors.pressedColor = new Color(buttonColor.r * 0.7f, buttonColor.g * 0.7f, buttonColor.b * 0.7f, 1f);
            colors.selectedColor = buttonColor;
            colors.fadeDuration = 0.1f;
            button.colors = colors;

            // Button Text
            var textObj = new GameObject("Text");
            textObj.transform.SetParent(obj.transform, false);
            var textRect = textObj.AddComponent<RectTransform>();
            textRect.anchorMin = Vector2.zero;
            textRect.anchorMax = Vector2.one;
            textRect.offsetMin = Vector2.zero;
            textRect.offsetMax = Vector2.zero;

            var tmp = textObj.AddComponent<TextMeshProUGUI>();
            tmp.text = text;
            tmp.fontSize = fontSize - 2;
            tmp.fontStyle = FontStyles.Bold;
            tmp.color = textColor;
            tmp.alignment = TextAlignmentOptions.Center;

            return button;
        }

        private void SetStatus(string message, Color color)
        {
            if (statusText != null)
            {
                statusText.text = message;
                statusText.color = color;
            }
        }

        private void TryConnect()
        {
            if (networkService == null)
            {
                networkService = ServiceLocator.Get<INetworkService>();
                if (networkService == null)
                {
                    SetStatus("NetworkService not found!", Color.red);
                    return;
                }
            }

            if (networkService.IsConnected)
            {
                SetStatus("Already connected!", Color.yellow);
                return;
            }

            // Parse port
            if (!int.TryParse(portInput.text, out int port) || port < 1 || port > 65535)
            {
                SetStatus("Invalid port (1-65535)", Color.red);
                return;
            }

            // Validate host
            string host = hostInput.text?.Trim();
            if (string.IsNullOrWhiteSpace(host))
            {
                SetStatus("Host cannot be empty", Color.red);
                return;
            }

            SetStatus("Connecting...", Color.yellow);
            Debug.Log($"ConnectionDialogUI: Connecting to {host}:{port}");
            networkService.Connect(host, port);
        }

        public void Show()
        {
            if (panel != null)
            {
                panel.SetActive(true);
                SetStatus("", Color.white);
            }
        }
    }
}
