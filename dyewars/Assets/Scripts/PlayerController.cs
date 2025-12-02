using UnityEngine;

public class PlayerController : MonoBehaviour
{
    public System.Action OnMoveComplete;
    
    [Header("Sprite Layers")]
    [SerializeField] private SpriteRenderer bodyRenderer;
    [SerializeField] private SpriteRenderer headRenderer;
    [SerializeField] private SpriteRenderer weaponRenderer;
    
    [Header("Sprite Sheets (sliced)")]
    [SerializeField] private Sprite[] bodySprites;
    [SerializeField] private Sprite[] headSprites;
    [SerializeField] private Sprite[] weaponSprites;
    
    [Header("Movement Settings")]
    [SerializeField] private float moveDuration = 0.35f;
    [SerializeField] private float turnCooldown = 0.2f;
    [SerializeField] private float cellSize = 1f;
    [SerializeField] float pivotGraceTime = 0.15f;
    
    public System.Action<int> OnQueuedDirectionReady;
    
    // Movement state
    public bool IsMoving => isMoving;
    public bool IsBusy => isMoving || cooldownTimer > 0f;
    
    private bool isMoving = false;
    private Vector3 startPosition;
    private Vector3 targetPosition;
    private float moveTimer = 0f;
    private float cooldownTimer = 0f;
    private int queuedDirection = -1;
    
    // Animation state
    private float frameTime => moveDuration / 4f;
    private int[] walkFrameSequence = { 0, 1, 0, 2 };
    private int currentFrameIndex = 0;
    private float animationTimer = 0f;
    private int currentFacing = 2;

    public void QueueDirection(int direction)
    {
        queuedDirection = direction;
    }
    
    private void Update()
    {
        if (cooldownTimer > 0f)
            cooldownTimer -= Time.deltaTime;
        

        if (isMoving)
        {
            moveTimer += Time.deltaTime;
            float progress = moveTimer / moveDuration;
            
            transform.position = Vector3.Lerp(startPosition, targetPosition, progress);
            
            animationTimer += Time.deltaTime;
            if (animationTimer >= frameTime)
            {
                animationTimer = 0f;
                currentFrameIndex = (currentFrameIndex + 1) % walkFrameSequence.Length;
                UpdateSprites(currentFacing, walkFrameSequence[currentFrameIndex]);
            }

            if (progress >= 1f)
            {
                isMoving = false;
                transform.position = targetPosition;
                UpdateSprites(currentFacing, 0);

                // Process queued direction
                if (queuedDirection != -1)
                {
                    int directionToExecute = queuedDirection;
                    queuedDirection = -1;  // Clear queue
                    OnQueuedDirectionReady?.Invoke(directionToExecute);
                }
            }
        }
    }

    public void SetFacing(int direction, float timeSinceInput = 999f)
    {
        currentFacing = direction;
        
        if (!isMoving)
        {
            UpdateSprites(currentFacing, 0);
            
            if (timeSinceInput > pivotGraceTime)
            {
                cooldownTimer = turnCooldown;
            }
        }

        
    }

    public void MoveTo(Vector2Int gridPos)
    {
        startPosition = transform.position;
        targetPosition = new Vector3(
            gridPos.x * cellSize + cellSize * 0.5f,
            gridPos.y * cellSize + cellSize * 0.5f,
            0
        );

        isMoving = true;
        moveTimer = 0f;
        cooldownTimer = 0f;
        currentFrameIndex = 0;
        animationTimer = 0f;
    }
    
    public void SnapToPosition(Vector2Int gridPos)
    {
        transform.position = new Vector3(
            gridPos.x * cellSize + cellSize * 0.5f,
            gridPos.y * cellSize + cellSize * 0.5f,
            0
        );
        isMoving = false;
        UpdateSprites(currentFacing, 0);
    }

    private int GetSpriteIndex(int facing, int frameOffset)
    {
        return (facing * 3) + frameOffset;
    }
    
    private void UpdateSprites(int facing, int frameOffset)
    {
        int index = GetSpriteIndex(facing, frameOffset);
        
        if (bodySprites != null && index < bodySprites.Length)
            bodyRenderer.sprite = bodySprites[index];
            
        if (headSprites != null && index < headSprites.Length)
        {
            headRenderer.sprite = headSprites[index];
            headRenderer.sortingOrder = (facing == 0) ? -1 : 1;
        }
        
        if (weaponSprites != null && index < weaponSprites.Length)
            weaponRenderer.sprite = weaponSprites[index];
    }
}