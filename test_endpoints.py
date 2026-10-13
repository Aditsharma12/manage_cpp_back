import requests
import json
import time
import sys

BASE_URL = "http://127.0.0.1:5000/api"

print("--- TESTING DROGON C++ BACKEND ENDPOINTS ---")

def test():
    ts = int(time.time())
    email1 = f"alice_{ts}@example.com"
    email2 = f"bob_{ts}@example.com"

    # 1. Register User 1
    print("\n1. Testing POST /api/auth/register (User 1)...")
    r1 = requests.post(f"{BASE_URL}/auth/register", json={
        "name": "Alice Smith",
        "roll_number": "CS101",
        "email": email1,
        "phone_number": "+1234567890",
        "password": "password123"
    })
    print(f"Status: {r1.status_code}, Body: {r1.text}")
    assert r1.status_code == 201, "Failed to register User 1"
    user1_id = r1.json().get("user_id")

    # 2. Register User 2
    print("\n2. Testing POST /api/auth/register (User 2)...")
    r2 = requests.post(f"{BASE_URL}/auth/register", json={
        "name": "Bob Jones",
        "roll_number": "CS102",
        "email": email2,
        "phone_number": "+1987654321",
        "password": "password456"
    })
    print(f"Status: {r2.status_code}, Body: {r2.text}")
    assert r2.status_code == 201, "Failed to register User 2"
    user2_id = r2.json().get("user_id")

    # 3. Login User 1
    print("\n3. Testing POST /api/auth/login (User 1)...")
    login1 = requests.post(f"{BASE_URL}/auth/login", json={
        "email": email1,
        "password": "password123"
    })
    print(f"Status: {login1.status_code}, Token present: {'access_token' in login1.json()}")
    assert login1.status_code == 200, "User 1 login failed"
    token1 = login1.json()["access_token"]
    headers1 = {"Authorization": f"Bearer {token1}"}

    # 4. Login User 2
    print("\n4. Testing POST /api/auth/login (User 2)...")
    login2 = requests.post(f"{BASE_URL}/auth/login", json={
        "email": email2,
        "password": "password456"
    })
    assert login2.status_code == 200, "User 2 login failed"
    token2 = login2.json()["access_token"]
    headers2 = {"Authorization": f"Bearer {token2}"}

    # 5. Update Profile (User 1)
    print("\n5. Testing PUT /api/users/profile (User 1)...")
    prof = requests.put(f"{BASE_URL}/users/profile", headers=headers1, json={
        "name": "Alice Smith Updated",
        "roll_number": "CS101-Honors",
        "phone_number": "+1234567899",
        "about": "Updated about section"
    })
    print(f"Status: {prof.status_code}, Name: {prof.json().get('name')}")
    assert prof.status_code == 200, "Update profile failed"

    # 6. Get Directory
    print("\n6. Testing GET /api/directory...")
    dir_res = requests.get(f"{BASE_URL}/directory", headers=headers1)
    print(f"Status: {dir_res.status_code}, Count: {len(dir_res.json())}")
    assert dir_res.status_code == 200, "Get directory failed"

    # 7. Create Group (User 1)
    print("\n7. Testing POST /api/groups (Create Group)...")
    grp_res = requests.post(f"{BASE_URL}/groups", headers=headers1, json={
        "name": "Algorithms Study Group",
        "purpose": "Solve LeetCode together",
        "max_capacity": 5
    })
    print(f"Status: {grp_res.status_code}, Group: {grp_res.json()}")
    assert grp_res.status_code == 201, "Create group failed"
    group_id = grp_res.json().get("id")

    # 8. Get My Groups (User 1)
    print("\n8. Testing GET /api/groups/my_groups...")
    my_grp = requests.get(f"{BASE_URL}/groups/my_groups", headers=headers1)
    print(f"Status: {my_grp.status_code}, My Groups Count: {len(my_grp.json())}")
    assert my_grp.status_code == 200, "Get my groups failed"

    # 9. Get Group Details
    print(f"\n9. Testing GET /api/groups/{group_id} (Group Details)...")
    grp_det = requests.get(f"{BASE_URL}/groups/{group_id}", headers=headers1)
    print(f"Status: {grp_det.status_code}, Members Count: {len(grp_det.json().get('members', []))}")
    assert grp_det.status_code == 200, "Get group details failed"

    # 10. Update Group
    print(f"\n10. Testing PUT /api/groups/{group_id} (Update Group)...")
    grp_upd = requests.put(f"{BASE_URL}/groups/{group_id}", headers=headers1, json={
        "name": "Advanced Algorithms",
        "purpose": "Solve Hard LeetCode"
    })
    print(f"Status: {grp_upd.status_code}, Body: {grp_upd.text}")
    assert grp_upd.status_code == 200, "Update group failed"

    # 11. Send Invite (User 1 -> User 2)
    print(f"\n11. Testing POST /api/invites (User 1 inviting User 2)...")
    inv_send = requests.post(f"{BASE_URL}/invites", headers=headers1, json={
        "group_id": group_id,
        "receiver_id": user2_id
    })
    print(f"Status: {inv_send.status_code}, Body: {inv_send.text}")
    assert inv_send.status_code == 201, "Send invite failed"

    # 12. Get Pending Invites (User 2)
    print("\n12. Testing GET /api/invites/pending (User 2)...")
    inv_pend = requests.get(f"{BASE_URL}/invites/pending", headers=headers2)
    print(f"Status: {inv_pend.status_code}, Pending count: {len(inv_pend.json())}")
    assert inv_pend.status_code == 200, "Get pending invites failed"
    assert len(inv_pend.json()) > 0, "No pending invite found"
    invite_id = inv_pend.json()[0]["id"]

    # 13. Respond to Invite (User 2 accepts)
    print(f"\n13. Testing POST /api/invites/{invite_id}/respond (User 2 accepts)...")
    inv_resp = requests.post(f"{BASE_URL}/invites/{invite_id}/respond", headers=headers2, json={
        "action": "accept"
    })
    print(f"Status: {inv_resp.status_code}, Body: {inv_resp.text}")
    assert inv_resp.status_code == 200, "Accept invite failed"

    # 14. Verify User 2 is now in group
    print(f"\n14. Verifying User 2 is in group...")
    grp_det2 = requests.get(f"{BASE_URL}/groups/{group_id}", headers=headers2)
    member_ids = grp_det2.json().get("member_ids", [])
    print(f"Group member IDs: {member_ids}")
    assert user2_id in member_ids, "User 2 not in member_ids!"

    # 15. Delete / Leave Group (User 2 leaves)
    print(f"\n15. Testing DELETE /api/groups/{group_id} (User 2 leaves)...")
    leave_res = requests.delete(f"{BASE_URL}/groups/{group_id}", headers=headers2)
    print(f"Status: {leave_res.status_code}, Body: {leave_res.text}")
    assert leave_res.status_code == 200, "User 2 leave group failed"

    # 16. Delete Group (User 1 permanently deletes)
    print(f"\n16. Testing DELETE /api/groups/{group_id} (User 1 deletes group)...")
    del_res = requests.delete(f"{BASE_URL}/groups/{group_id}", headers=headers1)
    print(f"Status: {del_res.status_code}, Body: {del_res.text}")
    assert del_res.status_code == 200, "User 1 delete group failed"

    print("\n=========================================")
    print(" ALL 16 TESTS PASSED SUCCESSFULLY! ")
    print("=========================================")

if __name__ == "__main__":
    test()
