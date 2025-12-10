using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using NUnit.Framework;
using DyeWars.Network.Protocol;

public class OpcodeTests
{
    // ====================================================================
    // Verify opcodes don't collide (uses reflection - auto-updates)
    // ====================================================================

    [Test]
    public void ClientOpcodes_NoDuplicates()
    {
        var opcodes = GetAllOpcodes().Where(op => op.Name.StartsWith("C_")).ToList();
        AssertNoDuplicates(opcodes, "client");
    }

    [Test]
    public void ServerOpcodes_NoDuplicates()
    {
        var opcodes = GetAllOpcodes().Where(op => op.Name.StartsWith("S_")).ToList();
        AssertNoDuplicates(opcodes, "server");
    }

    [Test]
    public void AllOpcodes_NoDuplicatesAcrossCategories()
    {
        var opcodes = GetAllOpcodes();
        AssertNoDuplicates(opcodes, "all");
    }

    // ====================================================================
    // Verify OpcodeUtil returns correct names (catches typos)
    // ====================================================================

    [Test]
    public void GetName_AllDefinedOpcodes_NotUnknown()
    {
        var opcodes = GetAllOpcodes();

        foreach (var op in opcodes)
        {
            string name = OpcodeUtil.GetName(op.Value);
            Assert.IsFalse(
                name.StartsWith("Unknown"),
                $"Opcode {op.FullName} (0x{op.Value:X2}) returned Unknown from OpcodeUtil.GetName()"
            );
        }
    }

    // ====================================================================
    // Verify client/server direction detection
    // ====================================================================

    [Test]
    public void IsClientToServer_AllClientOpcodes_ReturnsTrue()
    {
        var clientOpcodes = GetAllOpcodes().Where(op => op.Name.StartsWith("C_"));

        foreach (var op in clientOpcodes)
        {
            Assert.IsTrue(
                OpcodeUtil.IsClientToServer(op.Value),
                $"{op.FullName} (0x{op.Value:X2}) should be client-to-server"
            );
        }
    }

    [Test]
    public void IsServerToClient_AllServerOpcodes_ReturnsTrue()
    {
        var serverOpcodes = GetAllOpcodes().Where(op => op.Name.StartsWith("S_"));

        foreach (var op in serverOpcodes)
        {
            Assert.IsTrue(
                OpcodeUtil.IsServerToClient(op.Value),
                $"{op.FullName} (0x{op.Value:X2}) should be server-to-client"
            );
        }
    }

    // ====================================================================
    // Helpers
    // ====================================================================

    private struct OpcodeInfo
    {
        public string Name;      // e.g., "C_Move_Request"
        public string Category;  // e.g., "Movement"
        public string FullName;  // e.g., "Movement.C_Move_Request"
        public byte Value;
    }

    private static List<OpcodeInfo> GetAllOpcodes()
    {
        var result = new List<OpcodeInfo>();
        var nestedTypes = typeof(Opcode).GetNestedTypes(BindingFlags.Public);

        foreach (var nestedType in nestedTypes)
        {
            var fields = nestedType.GetFields(BindingFlags.Public | BindingFlags.Static)
                                   .Where(f => f.FieldType == typeof(byte));

            foreach (var field in fields)
            {
                result.Add(new OpcodeInfo
                {
                    Name = field.Name,
                    Category = nestedType.Name,
                    FullName = $"{nestedType.Name}.{field.Name}",
                    Value = (byte)field.GetValue(null)
                });
            }
        }

        return result;
    }

    private static void AssertNoDuplicates(List<OpcodeInfo> opcodes, string category)
    {
        var seen = new Dictionary<byte, OpcodeInfo>();

        foreach (var op in opcodes)
        {
            if (seen.TryGetValue(op.Value, out var existing))
            {
                Assert.Fail(
                    $"Duplicate {category} opcode 0x{op.Value:X2}: " +
                    $"{existing.FullName} and {op.FullName}"
                );
            }
            seen[op.Value] = op;
        }

        Assert.IsTrue(opcodes.Count > 0, $"No {category} opcodes found - check Opcode class structure");
    }
}
